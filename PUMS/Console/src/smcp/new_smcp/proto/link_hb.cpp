/**
 * @file new_smcp/proto/link_hb.cpp
 * @brief Тела LinkHb в порядке объявления.
 *
 * session.hpp включает link_hb.hpp и держит LinkHb членом, поэтому тела здесь.
 */

#include "smcp/new_smcp/proto/link_hb.hpp"

#include "smcp/new_smcp/pdu.hpp"
#include "smcp/new_smcp/session.hpp"

namespace smcp::proto {

LinkHb::LinkHb(Session& session) noexcept
    : Proto(session)
    , _session(session)
    , _timer(Heartbeat::kMissMax)
{}

bool LinkHb::isDown() const noexcept
{
    return _status == Status::Down || _status == Status::LostHb || _status == Status::PeerBusy;
}

LinkHb::Status LinkHb::status() const noexcept
{
    return _status;
}

bool LinkHb::isOpen() const noexcept
{
    return _status == Status::Up;
}

bool LinkHb::isMaster() const noexcept
{
    return _role == Role::Master;
}

void LinkHb::setStatus(Status st) noexcept
{
    const Status prev = _status;
    _status = st;
    if (st != prev) {
        _session.onLink(_status);
    }
}

void LinkHb::reset(Status st) noexcept
{
    _timer.clear();
    _txState = txState::Idle;
    _txMsg = Heartbeat::Msg::KeepAlive;
    setStatus(st);
}

void LinkHb::start() noexcept
{
    _role = Role::Master;
    reset(Status::Connecting);
}

void LinkHb::stop() noexcept
{
    _role = Role::Slave;
    reset(Status::Down);
}

void LinkHb::prepareTxMsg(Heartbeat::Msg msg) noexcept {
    _txMsg = msg;
    _txState = txState::Ready;
}

bool LinkHb::onPacket(const Packet& pkt, MISC::time_t now) noexcept
{
    SMCP_IF_MSG(Heartbeat) {
        if (_status == Status::LostHb || _status == Status::PeerBusy) {
            return true;
        }
        if (_status == Status::Down && _role == Role::Slave && body.msg == Heartbeat::Msg::Request) {
            setStatus(Status::Connecting);
            return true;
        }

        if (_status == Status::Connecting) {
            if ((_role == Role::Master && body.msg == Heartbeat::Msg::Ack) || 
                (_role == Role::Slave && body.msg == Heartbeat::Msg::KeepAlive)) {
                setStatus(Status::Up);
                return true;
            } else if (_role == Role::Slave && body.msg == Heartbeat::Msg::Request) {
                prepareTxMsg(Heartbeat::Msg::Ack);
                return true;
            } else if (_role == Role::Master && body.msg == Heartbeat::Msg::Busy) {
                setStatus(Status::PeerBusy);
                return true;
            }
        }

        if (_status == Status::Up) {
            _timer.clear();
            _timer.start(now, Heartbeat::kTimeoutMs, false);
            return true;
        }
    }
    return false;
}

void LinkHb::tick(MISC::time_t now) noexcept
{
    if (isDown()) {
        return;
    }
    
    if (_timer.isRunning() && !_timer.isWaiting()) {
        if (!_timer.timedOut(now)) {
            return;
        }
        _timer.clear();
    }

    if (!_timer.isRunning() || 
        (_role == Role::Master && _timer.timedOut(now) && !_timer.isAttemptsOver())) 
    {
        if (_txState == txState::Idle) {
            if (_status == Status::Connecting) {
                prepareTxMsg(_role == Role::Master ? Heartbeat::Msg::Request : Heartbeat::Msg::Ack);
            } 
            else if (_status == Status::Up) {
                prepareTxMsg(Heartbeat::Msg::KeepAlive);
            }
        } 
        else if (_txState == txState::Sent) {
            _timer.start(now, Heartbeat::kTimeoutMs, true);
            _txState = txState::Idle;
        }
        return;
    } 
    else if (_role == Role::Slave && _timer.timedOut(now) && !_timer.isAttemptsOver()) {
        _timer.start(now, Heartbeat::kTimeoutMs, true);
    }
    else if (_timer.isAttemptsOver() && _timer.timedOut(now)) {
        reset(Status::LostHb);
    }
}

void LinkHb::abort() noexcept
{
    stop();
}

bool LinkHb::peekTx(Packet& out, MISC::time_t) noexcept
{
    if (_txState == txState::Ready) {
        _txState = txState::Packed;
        out = {};
        out.dst = _session.peer();
        out.txn = 0;
        Heartbeat{_txMsg}.pack(out.msg);
        return true;
    }
    return false;
}

void LinkHb::onTxDone(const Packet&, bool ok) noexcept
{
    if (_txState == txState::Packed) {
        _txState = ok ? txState::Sent : txState::Ready;
    }
}

} // namespace smcp::proto
