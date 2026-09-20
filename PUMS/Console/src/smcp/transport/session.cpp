/**
 * @file session.cpp
 */

#include "smcp/transport/session.hpp"
#include "smcp/transport/node.hpp"
#include "smcp/debug.hpp"

#include <variant>

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
    reply.code = msg::ErrorCode::Timeout;
    reply.detail = msg::kNackDetailNone;
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
    if (!isOpen()) {
        return;
    }

    transmit(msg::Ack{}, req_pkt_id);
}

void Session::sendNack(uint8_t req_pkt_id, msg::ErrorCode code, uint8_t detail) noexcept
{
    if (!isOpen()) {
#if defined(SMCP_TRACE_SHORT)
        SMCP_SESS("S%u Nk! #%u %s %u\n",
                 static_cast<unsigned>(_id),
                 static_cast<unsigned>(req_pkt_id),
                 msg::cstrS(code),
                 static_cast<unsigned>(detail));
#else
        SMCP_SESS("[SMCP] Session[%u] sendNack drop (!Open) pkt=%u code=%s detail=%u\n",
                 static_cast<unsigned>(_id),
                 static_cast<unsigned>(req_pkt_id),
                 msg::cstr(code),
                 static_cast<unsigned>(detail));
#endif
        return;
    }
#if defined(SMCP_TRACE_SHORT)
    SMCP_SESS("S%u Nk p=%u #%u %s %u\n",
             static_cast<unsigned>(_id),
             static_cast<unsigned>(_peer_id),
             static_cast<unsigned>(req_pkt_id),
             msg::cstrS(code),
             static_cast<unsigned>(detail));
#else
    SMCP_SESS("[SMCP] Session[%u] sendNack peer=%u pkt=%u code=%s detail=%u\n",
             static_cast<unsigned>(_id),
             static_cast<unsigned>(_peer_id),
             static_cast<unsigned>(req_pkt_id),
             msg::cstr(code),
             static_cast<unsigned>(detail));
#endif
    msg::Nack body{};
    body.code = code;
    body.detail = detail;
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
    if (const auto* nack = std::get_if<msg::Nack>(&pkt.body)) {
        _node.onNack(this, *head, *nack);
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
    pkt.pkt_id = head != nullptr ? head->pkt_id : uint8_t{0};
    msg::Nack body{};
    body.code = msg::ErrorCode::Timeout;
    body.detail = msg::kNackDetailNone;
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
        setStatus(Status::Awaiting);
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
    setStatus(Status::Open);
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
