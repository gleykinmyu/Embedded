/**
 * @file link_hb.cpp
 */

#include "smcp/session/link_hb.hpp"

#include "smcp/debug.hpp"
#include "smcp/session.hpp"

namespace smcp {

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

void LinkHb::idle() noexcept
{
    _hb.clear();
    setStatus(Status::Idle);
}

void LinkHb::setStatus(Status status) noexcept
{
    if (status == _status) {
        return;
    }
    const bool was_open = (_status == Status::Open);
    SMCP_LOG(SMCP_SESS,
        "S%u %s>%s p=%u\n",
        "[SMCP] Session[%u] status %s -> %s peer=%u\n",
        static_cast<unsigned>(_s.id()),
        cstr(_status),
        cstr(status),
        static_cast<unsigned>(_s.peerId()));
    _status = status;
    const bool now_open = (_status == Status::Open);
    if (was_open != now_open) {
        _s.node.onLink(&_s, now_open);
    }
}

void LinkHb::ping(uint32_t now_ms) noexcept
{
    Message m{};
    m.id = msg::Heartbeat::kId;
    if (!_s.send(m)) {
        return;
    }
    _hb.start(now_ms, msg::Heartbeat::kTimeoutMs);
    if (_status != Status::Open) {
        setStatus(Status::Awaiting);
    }
}

void LinkHb::pong() noexcept
{
    Message m{};
    m.id = msg::Heartbeat::kId;
    _s.send(m);
}

bool LinkHb::onPacket(const Packet& pkt, uint32_t now_ms) noexcept
{
    if (pkt.msg.id != msg::Heartbeat::kId) {
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
    _hb.start(now_ms, msg::Heartbeat::kTimeoutMs, false);
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
            _s.close(Session::Fault::HbLost);
            return;
        }
        ping(now_ms);
        return;
    }

    if (_hb.isReplyLimit()) {
        _s.close(Session::Fault::HbLost);
        return;
    }
    _hb.start(now_ms, msg::Heartbeat::kTimeoutMs, false);
}

} // namespace smcp
