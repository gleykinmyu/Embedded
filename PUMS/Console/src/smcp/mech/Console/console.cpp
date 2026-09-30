/**
 * @file console.cpp
 * @brief IConsole: master Session TX, Telemetry RX.
 */

#include "smcp/mech/Console/console.hpp"

namespace smcp {

namespace detail {

void registerMech(IConsole& cons, CMech& mech) noexcept
{
    if (!msg::isServerId(mech.serverId())) {
        cons.setStatus(Node::Status::RegisterFailed);
        return;
    }
    IConsole::MechReg* const reg = cons.storage(mech.serverId());
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

IConsole::IConsole(ILink& link, ClockFn clock) noexcept
    : Node(link, clock)
{}

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

ServerSession* IConsole::server(uint8_t server_id) noexcept
{
    return static_cast<ServerSession*>(sessionByPeer(server_id));
}

ServerSession* IConsole::start(uint8_t server_id) noexcept
{
    if (!isReady() || !msg::isServerId(server_id)) {
        return nullptr;
    }

    Session* existing = sessionByPeer(server_id);
    if (existing != nullptr) {
        if (existing->getStatus() == Session::Status::Idle) {
            existing->start(server_id);
        }
        return static_cast<ServerSession*>(existing);
    }

    Session* const slot = Node::idleSession();
    if (slot == nullptr) {
        onSessionFull(server_id);
        return nullptr;
    }
    slot->start(server_id);
    return static_cast<ServerSession*>(slot);
}

ServerSession::ServerSession(IConsole& console) noexcept
    : Session(console)
{}

bool ServerSession::select(msg::Action action, Selection selection) noexcept
{
    if (selection.empty() && action != msg::Action::Set) {
        return false;
    }
    if (!isOpen()) {
        return false;
    }
    msg::Select body{};
    body.action = action;
    body.selection = selection;
    return send(body);
}

bool ServerSession::setSelection(Selection selection) noexcept
{
    return select(msg::Action::Set, selection);
}

bool ServerSession::clearSelection() noexcept
{
    return setSelection(Selection{});
}

bool ServerSession::block(msg::Action action, Selection selection) noexcept
{
    if (selection.empty() && action != msg::Action::Set) {
        return false;
    }
    if (!isOpen()) {
        return false;
    }
    msg::Block body{};
    body.action = action;
    body.selection = selection;
    return send(body);
}

bool ServerSession::setBlocked(Selection selection) noexcept
{
    return block(msg::Action::Set, selection);
}

bool ServerSession::clearBlocked() noexcept
{
    return setBlocked(Selection{});
}

bool ServerSession::setTarget(uint8_t mech_id, const MotionTarget& target) noexcept
{
    if (!isOpen()) {
        return false;
    }
    msg::SetTarget body{};
    body.mech_id = mech_id;
    body.target = target;
    return send(body);
}

bool ServerSession::getTelemetry(Selection selection) noexcept
{
    if (!isOpen()) {
        return false;
    }
    msg::GetTelemetry body{};
    body.selection = selection;
    return send(body);
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
    const Session* const s = sessionByPeer(src_id);
    if (s == nullptr || !s->isOpen()) {
        return;
    }
    MechReg* const bank = storage(src_id);
    if (bank == nullptr) {
        return;
    }
    CMech* m = bank->get(body.mech_id);
    if (m == nullptr) {
        return;
    }
    m->onTelemetry(src_id, body);
}

} // namespace smcp
