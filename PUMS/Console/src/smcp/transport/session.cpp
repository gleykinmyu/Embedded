/**
 * @file session.cpp
 */

#include "smcp/transport/session.hpp"
#include "smcp/transport/node.hpp"

#include <variant>

namespace smcp {

// --- ctor / helpers ---

Session::Session(Node& node) noexcept
    : _node(node)
{
    detail::registerSession(node, *this);
}

bool Session::nodeOk() const noexcept
{
    return _node.getStatus() != Node::Status::IdConflict;
}

// --- жизненный цикл ---

void Session::abortAck() noexcept
{
    if (!_ack.isWaiting()) {
        _ack.clear();
        return;
    }
    const TxSlot* head = peekReq(true);
    const uint8_t pkt_id = head != nullptr ? head->pkt_id : uint8_t{0};
    msg::Nack body{};
    body.code = msg::ErrorCode::Timeout;
    _ack.clear();
    _node.onReply(this, pkt_id, body);
}

void Session::open(uint8_t peer_id) noexcept
{
    if (!nodeOk() || peer_id == 0u) {
        return;
    }
    abortAck();
    _hb.clear();
    clearTx();
    _peer_id = peer_id;
    _status = Status::Connecting;
}

void Session::start(uint8_t peer_id) noexcept
{
    _master = true;
    open(peer_id);
}

void Session::close() noexcept
{
    abortAck();
    _hb.clear();
    _master = false;
    _peer_id = 0;
    _pkt_tx = 0;
    _status = Status::Idle;
    clearTx();
}

void Session::hbLost() noexcept
{
    _node.onHbLost(this);
    close();
}

// --- исходящие PDU ---

bool Session::transmit(const msg::Message& body, uint8_t pkt_id) noexcept
{
    if (!nodeOk() || _peer_id == 0u) {
        return false;
    }

    TxSlot item{};
    item.body = body;
    item.dst_id = _peer_id;
    item.pkt_id = pkt_id;

    const bool is_req = msg::helpers::requiresAck(msg::helpers::msgIdOf(body));
    const bool ok = is_req ? _tx_req.enqueue(item, _node) : _tx_ctrl.enqueue(item, _node);
    const bool full = is_req ? _tx_req.isFull() : _tx_ctrl.isFull();
    if (!ok && full) {
        _node.onTxFull(this);
    }
    return ok;
}

void Session::send(const msg::Message& body) noexcept
{
    if (!isOpen()) {
        return;
    }
    const msg::MsgId id = msg::helpers::msgIdOf(body);
    const bool need_ack = msg::helpers::requiresAck(id);
    const uint8_t pkt_id = need_ack ? _pkt_tx : uint8_t{0};
    if (transmit(body, pkt_id) && need_ack) {
        ++_pkt_tx;
    }
}

void Session::sendAck(uint8_t req_pkt_id) noexcept
{
    if (!isOpen()) return;

    transmit(msg::Ack{}, req_pkt_id);
}

void Session::sendNack(uint8_t req_pkt_id, msg::ErrorCode code) noexcept
{
    if (!isOpen()) return;
    msg::Nack body{};
    body.code = code;
    transmit(body, req_pkt_id);
}

void Session::clearTx() noexcept
{
    _tx_req.clear();
    _tx_ctrl.clear();
    _out = OutSrc::None;
    _rr_ctrl = true;
}

const TxSlot* Session::peekCtrl() noexcept
{
    if (const TxSlot* ctrl = _tx_ctrl.peek()) {
        _out = OutSrc::Ctrl;
        return ctrl;
    }
    return nullptr;
}

const TxSlot* Session::peekReq(bool allow_waiting) noexcept
{
    if (_ack.isWaiting() && !allow_waiting) {
        if (!_ack.timedOut(_node.clockMs()) || _ack.isReplyLimit()) {
            return nullptr;
        }
    }
    if (const TxSlot* req = _tx_req.peek()) {
        _out = OutSrc::Req;
        return req;
    }
    return nullptr;
}

const TxSlot* Session::peekTx() noexcept
{
    if (_rr_ctrl) {
        if (const TxSlot* p = peekCtrl()) {
            return p;
        }
        if (const TxSlot* p = peekReq()) {
            return p;
        }
    } else {
        if (const TxSlot* p = peekReq()) {
            return p;
        }
        if (const TxSlot* p = peekCtrl()) {
            return p;
        }
    }
    _out = OutSrc::None;
    return nullptr;
}

void Session::onTxResult(bool ok) noexcept
{
    if (!ok) {
        return;
    }

    switch (_out) {
    case OutSrc::Req:
        /* голова остаётся до onAck; start — первый раз или retry */
        _ack.start(_node.clockMs(), msg::kAckTimeoutControlMs);
        break;

    case OutSrc::Ctrl:
        _tx_ctrl.drop();
        break;

    case OutSrc::None:
        break;
    }

    /* следующий drain — другая очередь (если обе живы) */
    if (_out != OutSrc::None) {
        _rr_ctrl = (_out != OutSrc::Ctrl);
    }
}

void Session::onAck(const msg::Packet& pkt) noexcept
{
    if (!_ack.isWaiting()) {
        return;
    }
    const TxSlot* head = peekReq(true);
    if (head == nullptr) {
        return;
    }
    if (head->pkt_id != pkt.pkt_id) {
        _node.onPktIdMismatch(this, head->pkt_id, pkt.pkt_id);
        return;
    }
    _ack.clear();
    _tx_req.drop();
    _node.onReply(this, pkt.pkt_id, pkt.body);
}

void Session::onAckTimeout() noexcept
{
    if (!_ack.isWaiting() || !_ack.isReplyLimit()) {
        return;
    }

    const TxSlot* head = peekReq(true);
    msg::Packet pkt{};
    pkt.pkt_id = head != nullptr ? head->pkt_id : uint8_t{0};
    msg::Nack body{};
    body.code = msg::ErrorCode::Timeout;
    pkt.body = body;
    onAck(pkt);
}

// --- Heartbeat ---

void Session::ping() noexcept
{
    if (!transmit(msg::Heartbeat{})) {
        return;
    }
    if (_status != Status::Open) {
        _status = Status::Awaiting;
    }
    _hb.start(_node.clockMs(), msg::kHeartbeatTimeoutMs);
}

void Session::pong() noexcept
{
    transmit(msg::Heartbeat{});
}

void Session::onHeartbeat(const msg::Header& hdr) noexcept
{
    if (!nodeOk() || hdr.src_id == 0u) {
        return;
    }

    if (_status == Status::Idle || _status == Status::Connecting) {
        open(hdr.src_id);
    } else if (hdr.src_id != _peer_id) {
        return;
    }

    if (!_hb.isWaiting()) {
        pong();
    }
    _status = Status::Open;
    _hb.clear();
    _hb.start(_node.clockMs(), msg::kHeartbeatTimeoutMs, false);
}

// --- pump (Node::update) ---

bool Session::onPacket(const msg::Packet& pkt) noexcept
{
    if (std::get_if<msg::Heartbeat>(&pkt.body) != nullptr) {
        onHeartbeat(pkt.hdr);
        return true;
    }

    if (std::holds_alternative<msg::Ack>(pkt.body)
        || std::holds_alternative<msg::Nack>(pkt.body)) {
        onAck(pkt);
        return true;
    }

    return false;
}

void Session::tick() noexcept
{
    if (!nodeOk()) {
        return;
    }

    if (_ack.timedOut(_node.clockMs())) {
        onAckTimeout();
    }

    if (_master && _status == Status::Connecting) {
        ping();
    }

    if (_hb.timedOut(_node.clockMs())) {
        if (_master) {
            if (_hb.isWaiting() && _hb.isReplyLimit()) {
                hbLost();
                return;
            }
            ping();
        } else {
            if (_hb.isReplyLimit()) {
                hbLost();
            } else {
                _hb.start(_node.clockMs(), msg::kHeartbeatTimeoutMs, false);
            }
        }
    }
}

} // namespace smcp
