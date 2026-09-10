/**
 * @file console.cpp
 */

#include "smcp/Console/console.hpp"

#include <variant>

namespace smcp {

namespace detail {

void registerMech(IConsole& cons, IMech& mech) noexcept
{
    (void)cons.storage().registerAt(mech.id(), &mech);
}

} // namespace detail

IConsole::IConsole(ILink& link, ClockFn clock) noexcept
    : Node(link, clock)
{}

uint8_t IConsole::serverId() const noexcept
{
    const Session* s = primarySession();
    return s != nullptr ? s->peerId() : uint8_t{0};
}

void IConsole::startSession(uint8_t server_id) noexcept
{
    if (Session* s = primarySession()) {
        s->start(server_id);
    }
}

void IConsole::stopSession() noexcept
{
    if (Session* s = primarySession()) {
        s->close();
    }
}

bool IConsole::linkUp() const noexcept
{
    const Session* s = primarySession();
    return s != nullptr && s->isOpen();
}

void IConsole::select(msg::Action action, Selection selection) noexcept
{
    if (selection.empty() && action != msg::Action::Set) {
        return;
    }

    Session* s = primarySession();
    if (s == nullptr) {
        return;
    }

    msg::Select body{};
    body.action = action;
    body.selection = selection;
    s->send(body);
}

void IConsole::setSelection(Selection selection) noexcept
{
    select(msg::Action::Set, selection);
}

void IConsole::clearSelection() noexcept
{
    setSelection(Selection{});
}

void IConsole::block(msg::Action action, Selection selection) noexcept
{
    if (selection.empty() && action != msg::Action::Set) {
        return;
    }

    Session* s = primarySession();
    if (s == nullptr) {
        return;
    }

    msg::Block body{};
    body.action = action;
    body.selection = selection;
    s->send(body);
}

void IConsole::setBlocked(Selection selection) noexcept
{
    block(msg::Action::Set, selection);
}

void IConsole::clearBlocked() noexcept
{
    setBlocked(Selection{});
}

void IConsole::onPacket(const msg::Packet& pkt) noexcept
{
    if (const auto* tel = std::get_if<msg::Telemetry>(&pkt.body)) {
        handleTelemetry(pkt.hdr, *tel);
    }
}

void IConsole::handleTelemetry(const msg::Header& hdr, const msg::Telemetry& body) noexcept
{
    (void)hdr;
    (void)body;
}

} // namespace smcp
