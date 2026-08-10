/**
 * @file session.cpp
 */

#include "smcp/transport/session.hpp"
#include "smcp/transport/node.hpp"

#include <variant>

namespace smcp {

Session::Session(Node& node, uint8_t slot_id) noexcept
    : _node(node)
{
    detail::registerSession(node, *this, slot_id);
}

bool Session::nodeOk() const noexcept
{
    return _node.getStatus() != Node::Status::IdConflict;
}

void Session::markDown() noexcept
{
    _open = false;
    _awaiting = false;
    _timer.stop();
}

void Session::setPeerId(uint8_t peer_id) noexcept
{
    _peer_id = peer_id;
    markDown();
}

void Session::start() noexcept
{
    if (!nodeOk()) {
        return;
    }
    _started = true;
}

void Session::stop() noexcept
{
    _started = false;
    markDown();
}

void Session::transmit(const msg::Message& body, uint8_t pkt_id) noexcept
{
    if (!nodeOk() || _peer_id == 0u) {
        return;
    }

    _node.send(body, _peer_id, pkt_id);
}

void Session::send(const msg::Message& body) noexcept
{
    const msg::MsgId id = msg::helpers::msgIdOf(body);
    const uint8_t pkt_id = msg::helpers::requiresAck(id) ? _pkt_tx++ : uint8_t{0};
    transmit(body, pkt_id);
}

void Session::sendAck(uint8_t req_pkt_id) noexcept
{
    transmit(msg::Ack{}, req_pkt_id);
}

void Session::sendNack(uint8_t req_pkt_id, msg::ErrorCode code) noexcept
{
    msg::Nack body{};
    body.code = code;
    transmit(body, req_pkt_id);
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
    transmit(msg::Heartbeat{});
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

    _open = true;
    _timer.start(_node.clockMs(), msg::kHeartbeatTimeoutMs);

    if (_awaiting) {
        _awaiting = false;
        return;
    }

    transmit(msg::Heartbeat{});
}

void Session::tick() noexcept
{
    if (!nodeOk()) {
        return;
    }

    const uint32_t now_ms = _node.clockMs();

    if (_timer.timedOut(now_ms)) {
        markDown();

        if (_started && _peer_id != 0u) {
            sendPing();
        }
        return;
    }

    if (_started && _peer_id != 0u && !_timer.isRunning() && !_awaiting) {
        sendPing();
    }
}

} // namespace smcp
