/**
 * @file console.cpp
 * @brief IConsole: master Session TX, Telemetry RX.
 */

#include "smcp/mech/Console/console.hpp"

namespace smcp {

namespace detail {

void registerMech(IConsole& cons, CMech& mech) noexcept
{
    IConsole::MechReg* const reg = cons.segment(mech.serverId());
    if (reg == nullptr) {
        cons.setStatus(Node::Status::RegisterFailed);
        return;
    }
    const MISC::RegStatus st = reg->registerAt(mech.id(), &mech);
    if (st != MISC::RegStatus::Ok) {
        cons.setStatus(Node::Status::RegisterFailed);
    }
}

} // namespace detail

IConsole::IConsole(ILink& link, ClockFn clock, Session& primary) noexcept
    : Node(link, clock)
    , _primary(primary)
{}

IConsole::MechReg* IConsole::segment(uint8_t server_id) noexcept
{
    if (server_id == CMech::kPrimaryServer || _primary.peerId() == server_id) {
        return &storage();
    }
    return nullptr;
}

bool IConsole::linkUp() const noexcept
{
    return _primary.isOpen();
}

void IConsole::begin(uint8_t console_id) noexcept
{
    if (!msg::isConsoleId(console_id)) {
        return;
    }
    Node::begin(console_id);
}

void IConsole::end() noexcept
{
    Node::end();
}

void IConsole::start(uint8_t server_id) noexcept
{
    if (!isReady()) {
        return;
    }
    if (!msg::isServerId(server_id)) {
        return;
    }

    Session* existing = sessionByPeer(server_id);
    if (existing != nullptr) {
        if (existing->getStatus() == Session::Status::Idle) {
            existing->start(server_id);
        }
        return;
    }

    Session* const slot = idleSession();
    if (slot == nullptr) {
        onSessionFull(server_id);
        return;
    }
    slot->start(server_id);
}

void IConsole::stop(uint8_t server_id) noexcept
{
    Session* const s = sessionTo(server_id);
    const uint8_t peer = (s != nullptr && s->peerId() != 0) ? s->peerId() : server_id;
    if (peer == CMech::kPrimaryServer || !msg::isServerId(peer)) {
        return;
    }

    if (s != nullptr) {
        s->close(Fault::None);
    }
}

void IConsole::select(msg::Action action, Selection selection, uint8_t server_id) noexcept
{
    if (selection.empty() && action != msg::Action::Set) {
        return;
    }

    Session* const s = sessionTo(server_id);
    if (s == nullptr) {
        return;
    }
    msg::Select body{};
    body.action = action;
    body.selection = selection;
    s->send(body);
}

void IConsole::setSelection(Selection selection, uint8_t server_id) noexcept
{
    select(msg::Action::Set, selection, server_id);
}

void IConsole::clearSelection(uint8_t server_id) noexcept
{
    setSelection(Selection{}, server_id);
}

void IConsole::block(msg::Action action, Selection selection, uint8_t server_id) noexcept
{
    if (selection.empty() && action != msg::Action::Set) {
        return;
    }

    Session* const s = sessionTo(server_id);
    if (s == nullptr) {
        return;
    }
    msg::Block body{};
    body.action = action;
    body.selection = selection;
    s->send(body);
}

void IConsole::setBlocked(Selection selection, uint8_t server_id) noexcept
{
    block(msg::Action::Set, selection, server_id);
}

void IConsole::clearBlocked(uint8_t server_id) noexcept
{
    setBlocked(Selection{}, server_id);
}

void IConsole::setTarget(uint8_t mech_id, const MotionTarget& target, uint8_t server_id) noexcept
{
    Session* const s = sessionTo(server_id);
    if (s == nullptr) {
        return;
    }
    msg::SetTarget body{};
    body.mech_id = mech_id;
    body.target = target;
    s->send(body);
}

void IConsole::getTelemetry(Selection selection, uint8_t server_id) noexcept
{
    Session* const s = sessionTo(server_id);
    if (s == nullptr) {
        return;
    }
    msg::GetTelemetry body{};
    body.selection = selection;
    s->send(body);
}

void IConsole::onPacket(const Packet& pkt) noexcept
{
    SMCP_IF_MSG(msg::Telemetry) {
        onTelemetry(pkt.src_id, body);
        return;
    }
    Node::onPacket(pkt);
}

void IConsole::onTelemetry(uint8_t src_id, const msg::Telemetry& body) noexcept
{
    MechReg* seg = segment(src_id);
    if (seg == nullptr) {
        return;
    }
    CMech* m = seg->get(body.mech_id);
    if (m == nullptr) {
        return;
    }
    m->onTelemetry(src_id, body);
}

Session* IConsole::sessionTo(uint8_t server_id) noexcept
{
    if (server_id == CMech::kPrimaryServer) {
        return &_primary;
    }
    return sessionByPeer(server_id);
}

const Session* IConsole::sessionTo(uint8_t server_id) const noexcept
{
    return const_cast<IConsole*>(this)->sessionTo(server_id);
}

Session* IConsole::idleSession() noexcept
{
    if (_primary.getStatus() == Session::Status::Idle) {
        return &_primary;
    }
    SessionReg& reg = sessions();
    const uint8_t end = reg.endId();
    for (uint8_t sid = reg.firstId(); sid < end; ++sid) {
        Session* const s = reg.get(sid);
        if (s != nullptr && s != &_primary && s->getStatus() == Session::Status::Idle) {
            return s;
        }
    }
    return nullptr;
}

} // namespace smcp
