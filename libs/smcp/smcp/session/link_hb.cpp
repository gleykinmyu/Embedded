/**
 * @file link_hb.cpp
 */

#include "smcp/session/link_hb.hpp"

#include "smcp/debug.hpp"
#include "smcp/session.hpp"

namespace smcp::session {

LinkHb::LinkHb(Session& session) noexcept
    : _s(session)
{}

const char* LinkHb::cstr(Status status) noexcept
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

void LinkHb::connect() noexcept
{
    _hb.clear();
    setStatus(Status::Connecting);
}

void LinkHb::idle(Node::Link link) noexcept
{
    if (link == Node::Link::Up) {
        link = Node::Link::Down;
    }
    _down = link;
    _hb.clear();
    setStatus(Status::Idle);
}

void LinkHb::setStatus(Status status) noexcept
{
    if (status == _status) {
        return;
    }
    const bool was_idle = (_status == Status::Idle);
    const bool was_open = (_status == Status::Open);
    SMCP_LOG(SMCP_SESS,
        "S %s>%s p=%u\n",
        "[SMCP] Session status %s -> %s peer=%u\n",
        cstr(_status),
        cstr(status),
        static_cast<unsigned>(_s.peerId()));
    _status = status;
    const bool now_idle = (_status == Status::Idle);
    const bool now_open = (_status == Status::Open);
    if (now_open && !was_open) {
        _s.node.onLink(&_s, Node::Link::Up);
    } else if (now_idle && !was_idle) {
        _s.node.onLink(&_s, _down);
        _down = Node::Link::Down;
    }
}

void LinkHb::ping(uint32_t now_ms) noexcept
{
    Message m{};
    m.id = Heartbeat::kId;
    if (!_s.send(m)) {
        return;
    }
    _hb.start(now_ms, Heartbeat::kTimeoutMs);
    if (_status != Status::Open) {
        setStatus(Status::Awaiting);
    }
}

void LinkHb::pong() noexcept
{
    Message m{};
    m.id = Heartbeat::kId;
    _s.send(m);
}

bool LinkHb::onPacket(const Packet& pkt, uint32_t now_ms) noexcept
{
    if (pkt.msg.id != Heartbeat::kId) {
        return false;
    }
    const uint8_t src_id = pkt.src_id;

    if (_status == Status::Idle || _status == Status::Connecting) {
        _s.open(src_id); /* bind peer + connect(); режет !ready / src==0 */
        if (_status == Status::Idle) {
            return true;
        }
    } else if (src_id != _s.peerId()) {
        return true;
    }

    if (!_hb.isWaiting()) {
        pong();
    }
    _hb.clear();
    _hb.start(now_ms, Heartbeat::kTimeoutMs, false);
    setStatus(Status::Open);
    return true;
}

void LinkHb::tick(uint32_t now_ms) noexcept
{
    if (_s.isMaster() && _status == Status::Connecting) {
        ping(now_ms);
    }

    if (!_hb.timedOut(now_ms)) {
        return;
    }

    if (_s.isMaster()) {
        if (_hb.isWaiting() && _hb.isReplyLimit()) {
            _s.close(Session::Link::HbLost);
            return;
        }
        ping(now_ms);
        return;
    }

    if (_hb.isReplyLimit()) {
        _s.close(Session::Link::HbLost);
        return;
    }
    _hb.start(now_ms, Heartbeat::kTimeoutMs, false);
}

} // namespace smcp::session
