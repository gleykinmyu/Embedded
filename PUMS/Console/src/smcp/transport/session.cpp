/**
 * @file session.cpp
 */

#include "smcp/transport/session.hpp"
#include "smcp/transport/node.hpp"

#include <variant>

namespace smcp {

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
    _status = status;
}

void Session::closeLink() noexcept
{
    _awaiting = false;
    _timer.stop();
    _tx.clear();

    /* stop() уже Idle — не поднимать Connecting. */
    if (_status != Status::Idle) {
        setStatus(Status::Connecting);
    }
}

void Session::setPeerId(uint8_t peer_id) noexcept
{
    _peer_id = peer_id;
    closeLink();
}

void Session::start() noexcept
{
    if (!nodeOk()) {
        return;
    }
    if (_status == Status::Idle) {
        setStatus(Status::Connecting);
    }
}

void Session::stop() noexcept
{
    setStatus(Status::Idle); /* до closeLink, иначе reconnect */
    closeLink();
}

bool Session::transmit(const msg::Message& body, uint8_t pkt_id) noexcept
{
    if (!nodeOk() || _peer_id == 0u) {
        return false;
    }

    TxSlot item{};
    item.body = body;
    item.dst_id = _peer_id;
    item.pkt_id = pkt_id;
    const bool ok = _tx.enqueue(item, _node);
    if (!ok && _tx.isFull()) {
        _node.onTxFull(this); /* каждая попытка, не только первый вход */
    }
    return ok;
}

void Session::send(const msg::Message& body) noexcept
{
    const msg::MsgId id = msg::helpers::msgIdOf(body);
    const bool need_ack = msg::helpers::requiresAck(id);
    const uint8_t pkt_id = need_ack ? _pkt_tx : uint8_t{0};
    if (transmit(body, pkt_id) && need_ack) {
        ++_pkt_tx; /* отказ enqueue не сжигает id */
    }
}

void Session::sendAck(uint8_t req_pkt_id) noexcept
{
    (void)transmit(msg::Ack{}, req_pkt_id);
}

void Session::sendNack(uint8_t req_pkt_id, msg::ErrorCode code) noexcept
{
    msg::Nack body{};
    body.code = code;
    (void)transmit(body, req_pkt_id);
}

void Session::onTxResult(bool ok) noexcept
{
    (void)ok;
    /* Позже: pending/Ack retry по голове. */
}

bool Session::onPacket(const msg::Packet& pkt) noexcept
{
    if (std::get_if<msg::Heartbeat>(&pkt.body) != nullptr) {
        onHeartbeat(pkt.hdr);
        return true;
    }

    /* Позже: снять pending по pkt.pkt_id. */
    if (std::get_if<msg::Ack>(&pkt.body) != nullptr
        || std::get_if<msg::Nack>(&pkt.body) != nullptr) {
        return true;
    }

    return false;
}

void Session::sendPing() noexcept
{
    if (!transmit(msg::Heartbeat{})) {
        return; /* без awaiting/timer — tick повторит ping */
    }
    _awaiting = true;
    _timer.start(_node.clockMs(), msg::kHeartbeatTimeoutMs);
}

void Session::onHeartbeat(const msg::Header& hdr) noexcept
{
    if (!nodeOk() || hdr.src_id == 0u) {
        return;
    }

    if (_peer_id == 0u) {
        _peer_id = hdr.src_id;
    } else if (hdr.src_id != _peer_id) {
        return;
    }

    setStatus(Status::Open);
    _timer.start(_node.clockMs(), msg::kHeartbeatTimeoutMs);

    if (_awaiting) {
        _awaiting = false; /* pong на наш ping — не отвечать */
        return;
    }

    (void)transmit(msg::Heartbeat{}); /* чужой ping → один pong */
}

void Session::tick() noexcept
{
    if (!nodeOk()) {
        return;
    }

    const uint32_t now_ms = _node.clockMs();

    if (_timer.timedOut(now_ms)) {
        closeLink();

        if (_status != Status::Idle && _peer_id != 0u) {
            sendPing(); /* reconnect */
        }
        return;
    }

    /* start() не шлёт сам: нет таймера и не ждём pong. */
    if (_status != Status::Idle && _peer_id != 0u && !_timer.isRunning() && !_awaiting) {
        sendPing();
    }
}

} // namespace smcp
