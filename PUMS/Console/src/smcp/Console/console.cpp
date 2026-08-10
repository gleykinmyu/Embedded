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

IConsole::IConsole(BIF::CAN::ICAN& can, uint8_t console_id) noexcept
    : Node(can, console_id)
{}

void IConsole::setServerId(uint8_t server_id) noexcept
{
    if (Session* s = primarySession()) {
        s->setPeerId(server_id);
    }
}

uint8_t IConsole::serverId() const noexcept
{
    const Session* s = primarySession();
    return s != nullptr ? s->peerId() : uint8_t{0};
}

void IConsole::startSession() noexcept
{
    if (Session* s = primarySession()) {
        s->start();
    }
}

void IConsole::stopSession() noexcept
{
    if (Session* s = primarySession()) {
        s->stop();
    }
}

bool IConsole::linkUp() const noexcept
{
    const Session* s = primarySession();
    return s != nullptr && s->isOpen();
}

void IConsole::select(msg::Select::Action action, Selection selection) noexcept
{
    if (selection.empty() && action != msg::Select::Action::Set) {
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
    select(msg::Select::Action::Set, selection);
}

void IConsole::clearSelection() noexcept
{
    setSelection(Selection{});
}

void IConsole::onPacket(const msg::Packet& pkt) noexcept
{
    if (const auto* tel = std::get_if<msg::Telemetry>(&pkt.body)) {
        handleTelemetry(pkt.hdr, *tel);
    }
}

void IConsole::handleTelemetry(const msg::Header& hdr, const msg::Telemetry& body) noexcept
{
    IMech* m = mech(body.mech_id);
    if (m == nullptr) {
        return;
    }
    m->onTelemetry(hdr.src_id, body);
}

} // namespace smcp
