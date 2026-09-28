/**
 * @file session.cpp
 */

#include "smcp/session.hpp"
#include "smcp/node.hpp"
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

void Session::setStatus(Status status) noexcept
{
    if (status == _status) {
        return;
    }
    const bool was_open = (_status == Status::Open);
    SMCP_LOG(SMCP_SESS,
        "S%u %s>%s p=%u\n",
        "[SMCP] Session[%u] status %s -> %s peer=%u\n",
        static_cast<unsigned>(_id),
        cstr(_status),
        cstr(status),
        static_cast<unsigned>(_peer_id));
    _status = status;
    if (was_open != (_status == Status::Open) && _peer_id != 0u) {
        _node.onLink(this, _status == Status::Open);
    }
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
    if (!_node.isReady() || peer_id == 0u) {
        SMCP_LOG(SMCP_SESS,
            "S%u rej p=%u rdy=%u\n",
            "[SMCP] Session[%u] open reject peer=%u ready=%u\n",
            static_cast<unsigned>(_id),
            static_cast<unsigned>(peer_id),
            _node.isReady() ? 1u : 0u);
        return;
    }
    abortAck();
    _hb.clear();
    clearTx();
    _pkt_tx = 0;
    setStatus(Status::Connecting);
    _peer_id = peer_id;
}

void Session::start(uint8_t peer_id) noexcept
{
    _master = true;
    open(peer_id);
}

void Session::close(Fault reason) noexcept
{
    if (_status == Status::Idle) {
        return;
    }
    if (reason != Fault::None) {
        onFault(reason);
    }
    abortAck();
    _hb.clear();
    _master = false;
    _pkt_tx = 0;
    setStatus(Status::Idle);
    _peer_id = 0;
    clearTx();
}

void Session::onFault(Fault reason) noexcept
{
    _node.onFault(this, reason);
}

// --- исходящие PDU ---

bool Session::transmit(Message msg, uint8_t pkt_id, bool needs_ack) noexcept
{
    if (!_node.isReady() || _peer_id == 0u) {
        return false;
    }

    if (msg.dlc > Message::kMaxDlc) {
        msg.dlc = Message::kMaxDlc;
    }

    TxSlot item{};
    item.msg = msg;
    item.dst_id = _peer_id;
    item.pkt_id = static_cast<uint8_t>(pkt_id & protocol::kPktIdMax);
    item.needs_ack = needs_ack;

    const bool ok = needs_ack ? _tx_req.enqueue(item, _node) : _tx_ctrl.enqueue(item, _node);
    const bool full = needs_ack ? _tx_req.isFull() : _tx_ctrl.isFull();
    if (!ok && full) {
        _node.onTxFull(this);
    }
    return ok;
}

void Session::send(Message msg, bool needs_ack) noexcept
{
    if (!isOpen()) {
        return;
    }
    const uint8_t pkt_id = needs_ack ? _pkt_tx : uint8_t{0};
    if (transmit(msg, pkt_id, needs_ack) && needs_ack) {
        _pkt_tx = static_cast<uint8_t>((_pkt_tx + 1u) & protocol::kPktIdMax);
    }
}

void Session::sendAck(uint8_t req_pkt_id) noexcept
{
    if (!isOpen()) {
        return;
    }

    Message m{};
    m.id = msg::Ack::kId;
    transmit(m, req_pkt_id, false);
}

void Session::sendNack(uint8_t req_pkt_id, uint8_t error, uint32_t detail) noexcept
{
    if (!isOpen()) {
        SMCP_LOG(SMCP_SESS,
            "S%u Nk! #%u %u %08lX\n",
            "[SMCP] Session[%u] sendNack drop (!Open) pkt=%u error=%u detail=0x%08lX\n",
            static_cast<unsigned>(_id),
            static_cast<unsigned>(req_pkt_id),
            static_cast<unsigned>(error),
            static_cast<unsigned long>(detail));
        return;
    }
    SMCP_LOG(SMCP_SESS,
        "S%u Nk p=%u #%u %u %08lX\n",
        "[SMCP] Session[%u] sendNack peer=%u pkt=%u error=%u detail=0x%08lX\n",
        static_cast<unsigned>(_id),
        static_cast<unsigned>(_peer_id),
        static_cast<unsigned>(req_pkt_id),
        static_cast<unsigned>(error),
        static_cast<unsigned long>(detail));
    msg::Nack body{};
    body.error = error;
    body.detail = detail;
    Message m{};
    m.id = msg::Nack::kId;
    if (!body.pack(m)) {
        _node.onPackFailed(this, msg::Nack::kId);
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

void Session::onAck(const Packet& pkt) noexcept
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

    SMCP_IF_MSG(msg::Nack) {
        _ack.clear();
        _node.onNack(this, *head, body);
        _tx_req.drop();
    } else SMCP_IF_MSG(msg::Ack) {
        _ack.clear();
        _node.onAck(this, *head, body);
        _tx_req.drop();
    }
}

void Session::onAckTimeout() noexcept
{
    if (!_ack.isWaiting() || !_ack.isReplyLimit()) {
        return;
    }
    abortAck();
}

// --- Heartbeat ---

void Session::ping() noexcept
{
    Message m{};
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
    Message m{};
    m.id = msg::Heartbeat::kId;
    transmit(m, 0, false);
}

void Session::onHeartbeat(uint8_t src_id) noexcept
{
    if (!_node.isReady() || src_id == 0u) {
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

bool Session::onPacket(const Packet& pkt) noexcept
{
    if (!pkt.msg.isTransport()) {
        return false;
    }
    if (pkt.msg.id == msg::Heartbeat::kId) {
        onHeartbeat(pkt.src_id);
        return true;
    }
    if (pkt.msg.id == msg::Ack::kId || pkt.msg.id == msg::Nack::kId) {
        onAck(pkt);
        return true;
    }
    return false; /* Fault — Node::onPacket */
}

void Session::tick() noexcept
{
    if (!_node.isReady()) {
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
                close(Fault::HbLost);
                return;
            }
            ping();
        } else {
            if (_hb.isReplyLimit()) {
                close(Fault::HbLost);
            } else {
                _hb.start(_node.clockMs(), msg::Heartbeat::kTimeoutMs, false);
            }
        }
    }
}

} // namespace smcp
