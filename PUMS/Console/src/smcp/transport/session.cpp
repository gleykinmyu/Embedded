/**
 * @file session.cpp
 */

#include "smcp/transport/session.hpp"
#include "smcp/transport/node.hpp"
#include "smcp/debug.hpp"

namespace smcp {

// --- ctor / helpers ---

const char* Session::cstr(Status status) noexcept
{
    switch (status) {
    case Status::Idle: return "Idle";
#if defined(SMCP_TRACE_SHORT)
    case Status::Connecting: return "Conn";
    case Status::Awaiting: return "Wait";
#else
    case Status::Connecting: return "Connecting";
    case Status::Awaiting: return "Awaiting";
#endif
    case Status::Open: return "Open";
    }
    return "?";
}

Session::Session(Node& node) noexcept
    : _node(node)
{
    detail::registerSession(node, *this);
}

bool Session::nodeOk() const noexcept
{
    return _node.getStatus() != Node::Status::IdConflict;
}

void Session::setStatus(Status status) noexcept
{
    if (status == _status) {
        return;
    }
#if defined(SMCP_TRACE_SHORT)
    SMCP_SESS("S%u %s>%s p=%u\n",
#else
    SMCP_SESS("[SMCP] Session[%u] status %s -> %s peer=%u\n",
#endif
             static_cast<unsigned>(_id),
             cstr(_status),
             cstr(status),
             static_cast<unsigned>(_peer_id));
    _status = status;
}

// --- жизненный цикл ---

void Session::abortAck() noexcept
{
    if (!_ack.isWaiting()) {
        _ack.clear();
        return;
    }
    const TxSlot* head = peekReq(true);
    _ack.clear();
    if (head == nullptr) {
        return;
    }
    msg::Nack reply{};
    reply.error = msg::Nack::kTimeout;
    reply.detail = msg::Nack::kDetailNone;
    _node.onNack(this, *head, reply);
    _tx_req.drop();
}

void Session::open(uint8_t peer_id) noexcept
{
    if (!nodeOk() || peer_id == 0u) {
#if defined(SMCP_TRACE_SHORT)
        SMCP_SESS("S%u rej p=%u ok=%u\n",
#else
        SMCP_SESS("[SMCP] Session[%u] open reject peer=%u nodeOk=%u\n",
#endif
                 static_cast<unsigned>(_id),
                 static_cast<unsigned>(peer_id),
                 nodeOk() ? 1u : 0u);
        return;
    }
    abortAck();
    _hb.clear();
    clearTx();
    _peer_id = peer_id;
    setStatus(Status::Connecting);
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
    _pkt_tx = 0;
    setStatus(Status::Idle);
    _peer_id = 0;
    clearTx();
}

void Session::hbLost() noexcept
{
    _node.onHbLost(this);
    close();
}

// --- исходящие PDU ---

bool Session::transmit(msg::Message msg, uint8_t pkt_id, bool needs_ack) noexcept
{
    if (!nodeOk() || _peer_id == 0u) {
        return false;
    }

    if (msg.dlc > msg::Message::kMaxDlc) {
        msg.dlc = msg::Message::kMaxDlc;
    }

    TxSlot item{};
    item.msg = msg;
    item.dst_id = _peer_id;
    item.pkt_id = static_cast<uint8_t>(pkt_id & msg::kPktIdMax);
    item.needs_ack = needs_ack;

    const bool ok = needs_ack ? _tx_req.enqueue(item, _node) : _tx_ctrl.enqueue(item, _node);
    const bool full = needs_ack ? _tx_req.isFull() : _tx_ctrl.isFull();
    if (!ok && full) {
        _node.onTxFull(this);
    }
    return ok;
}

void Session::send(msg::Message msg, bool needs_ack) noexcept
{
    if (!isOpen()) {
        return;
    }
    const uint8_t pkt_id = needs_ack ? _pkt_tx : uint8_t{0};
    if (transmit(msg, pkt_id, needs_ack) && needs_ack) {
        _pkt_tx = static_cast<uint8_t>((_pkt_tx + 1u) & msg::kPktIdMax);
    }
}

void Session::sendAck(uint8_t req_pkt_id) noexcept
{
    if (!isOpen()) {
        return;
    }

    msg::Message m{};
    m.id = msg::Ack::kId;
    transmit(m, req_pkt_id, false);
}

void Session::sendNack(uint8_t req_pkt_id, uint8_t error, uint8_t detail) noexcept
{
    if (!isOpen()) {
#if defined(SMCP_TRACE_SHORT)
        SMCP_SESS("S%u Nk! #%u %u %u\n",
                 static_cast<unsigned>(_id),
                 static_cast<unsigned>(req_pkt_id),
                 static_cast<unsigned>(error),
                 static_cast<unsigned>(detail));
#else
        SMCP_SESS("[SMCP] Session[%u] sendNack drop (!Open) pkt=%u error=%u detail=%u\n",
                 static_cast<unsigned>(_id),
                 static_cast<unsigned>(req_pkt_id),
                 static_cast<unsigned>(error),
                 static_cast<unsigned>(detail));
#endif
        return;
    }
#if defined(SMCP_TRACE_SHORT)
    SMCP_SESS("S%u Nk p=%u #%u %u %u\n",
             static_cast<unsigned>(_id),
             static_cast<unsigned>(_peer_id),
             static_cast<unsigned>(req_pkt_id),
             static_cast<unsigned>(error),
             static_cast<unsigned>(detail));
#else
    SMCP_SESS("[SMCP] Session[%u] sendNack peer=%u pkt=%u error=%u detail=%u\n",
             static_cast<unsigned>(_id),
             static_cast<unsigned>(_peer_id),
             static_cast<unsigned>(req_pkt_id),
             static_cast<unsigned>(error),
             static_cast<unsigned>(detail));
#endif
    msg::Nack body{};
    body.error = error;
    body.detail = detail;
    msg::Message m{};
    m.id = msg::Nack::kId;
    if (!body.pack(m)) {
        return;
    }
    transmit(m, req_pkt_id, false);
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
        _ack.start(_node.clockMs(), msg::Ack::kTimeoutMs);
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
    if (pkt.msg.id == msg::Nack::kId) {
        msg::Nack nack{};
        if (!nack.unpack(pkt.msg)) {
            _tx_req.drop();
            return;
        }
        _node.onNack(this, *head, nack);
    } else {
        _node.onAck(this, *head);
    }
    _tx_req.drop();
}

void Session::onAckTimeout() noexcept
{
    if (!_ack.isWaiting() || !_ack.isReplyLimit()) {
        return;
    }

    const TxSlot* head = peekReq(true);
    msg::Packet pkt{};
    pkt.msg.id = msg::Nack::kId;
    pkt.pkt_id = head != nullptr ? head->pkt_id : uint8_t{0};
    msg::Nack body{};
    body.error = msg::Nack::kTimeout;
    body.detail = msg::Nack::kDetailNone;
    if (!body.pack(pkt.msg)) {
        return;
    }
    onAck(pkt);
}

// --- Heartbeat ---

void Session::ping() noexcept
{
    msg::Message m{};
    m.id = msg::Heartbeat::kId;
    if (!transmit(m, 0, false)) {
        return;
    }
    if (_status != Status::Open) {
        setStatus(Status::Awaiting);
    }
    _hb.start(_node.clockMs(), msg::Heartbeat::kTimeoutMs);
}

void Session::pong() noexcept
{
    msg::Message m{};
    m.id = msg::Heartbeat::kId;
    transmit(m, 0, false);
}

void Session::onHeartbeat(uint8_t src_id) noexcept
{
    if (!nodeOk() || src_id == 0u) {
        return;
    }

    if (_status == Status::Idle || _status == Status::Connecting) {
        open(src_id);
    } else if (src_id != _peer_id) {
        return;
    }

    if (!_hb.isWaiting()) {
        pong();
    }
    setStatus(Status::Open);
    _hb.clear();
    _hb.start(_node.clockMs(), msg::Heartbeat::kTimeoutMs, false);
}

// --- pump (Node::update) ---

bool Session::onPacket(const msg::Packet& pkt) noexcept
{
    if (!pkt.msg.isTransport()) {
        return false;
    }
    if (pkt.msg.id == msg::Heartbeat::kId) {
        onHeartbeat(pkt.src_id);
        return true;
    }
    onAck(pkt);
    return true;
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
                _hb.start(_node.clockMs(), msg::Heartbeat::kTimeoutMs, false);
            }
        }
    }
}

} // namespace smcp
