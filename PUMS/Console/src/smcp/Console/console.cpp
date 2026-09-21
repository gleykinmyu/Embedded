/**
 * @file console.cpp
 * @brief IConsole: Phase lifecycle, primary Session TX, Telemetry RX.
 */

#include "smcp/Console/console.hpp"
#include "smcp/debug.hpp"

namespace smcp {

namespace detail {

void registerMech(IConsole& cons, CMech& mech) noexcept
{
    const MISC::RegStatus st = cons.storage().registerAt(mech.id(), &mech);
    if (st != MISC::RegStatus::Ok) {
        cons.setStatus(Node::Status::RegisterFailed);
    }
}

} // namespace detail

const char* IConsole::cstr(Phase phase) noexcept
{
    switch (phase) {
    case Phase::Idle: return "Idle";
    case Phase::Listen: return "Listen";
    case Phase::Connecting: return "Connecting";
    case Phase::Online: return "Online";
    case Phase::Fault: return "Fault";
    }
    return "?";
}

IConsole::IConsole(ILink& link, ClockFn clock, Session& primary) noexcept
    : Node(link, clock)
    , _primary(primary)
{}

void IConsole::update() noexcept
{
    Node::update();

    if (getStatus() == Status::IdConflict || getStatus() == Status::RegisterFailed) {
        if (_phase != Phase::Fault) {
            enterFault();
        }
        return;
    }

    switch (_phase) {
    case Phase::Idle:
    case Phase::Fault:
        break;

    case Phase::Listen:
        if (!_listen.timedOut(clockMs())) {
            break;
        }
        _listen.stop();
        if (getStatus() != Status::OK) {
            enterFault();
            break;
        }
        /* Тишина на шине — можно открывать primary. */
        startPrimary();
        setPhase(Phase::Connecting);
        break;

    case Phase::Connecting:
        tryGoOnline();
        if (_phase == Phase::Online) {
            break;
        }
        if (_primary.getStatus() == Session::Status::Idle && _server_id != 0u) {
            startPrimary();
        }
        break;

    case Phase::Online:
        if (!linkUp()) {
            setPhase(Phase::Connecting);
        }
        break;
    }
}

bool IConsole::linkUp() const noexcept
{
    return _primary.isOpen();
}

void IConsole::setConsoleId(uint8_t console_id) noexcept
{
    if (console_id == id()) {
        return;
    }
    link().setNodeId(console_id);
    if (_server_id != 0u) {
        begin(_server_id);
    } else {
        _primary.close();
        _listen.stop();
        setPhase(Phase::Idle);
    }
}

void IConsole::begin(uint8_t server_id) noexcept
{
    if (server_id == 0u) {
        return;
    }
    _server_id = server_id;
    _primary.close();
    if (getStatus() != Status::OK) {
        clearError();
    }
    _listen.start(clockMs(), msg::Heartbeat::kTimeoutMs);
    setPhase(Phase::Listen);
}

void IConsole::select(msg::Action action, Selection selection) noexcept
{
    if (selection.empty() && action != msg::Action::Set) {
        return;
    }

    msg::Select body{};
    body.action = action;
    body.selection = selection;
    _primary.send(body);
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

    msg::Block body{};
    body.action = action;
    body.selection = selection;
    _primary.send(body);
}

void IConsole::setBlocked(Selection selection) noexcept
{
    block(msg::Action::Set, selection);
}

void IConsole::clearBlocked() noexcept
{
    setBlocked(Selection{});
}

void IConsole::setTarget(uint8_t mech_id, const MotionTarget& target) noexcept
{
    msg::SetTarget body{};
    body.mech_id = mech_id;
    body.target = target;
    _primary.send(body);
}

void IConsole::getTelemetry(Selection selection) noexcept
{
    msg::GetTelemetry body{};
    body.selection = selection;
    _primary.send(body);
}

void IConsole::onPacket(const msg::Packet& pkt) noexcept
{
    (void)msg::helpers::take<msg::Telemetry>(pkt, [&](const msg::Telemetry& tel) {
        onTelemetry(pkt.src_id, tel);
    });
}

void IConsole::onStatus(Status status) noexcept
{
    Node::onStatus(status);
    switch (status) {
    case Status::OK:
    case Status::LinkError:
        break;

    case Status::RegisterFailed:
    case Status::IdConflict:
        enterFault();
        break;
    }
}

void IConsole::onHbLost(Session* session) noexcept
{
    if (session != &_primary) {
        return;
    }
    setPhase(Phase::Connecting);
}

void IConsole::onTelemetry(uint8_t src_id, const msg::Telemetry& body) noexcept
{
    CMech* m = mech(body.mech_id);
    if (m == nullptr) {
        return;
    }
    m->onTelemetry(src_id, body);
}

void IConsole::tryGoOnline() noexcept
{
    if (_phase == Phase::Connecting && linkUp() && readyForOnline()) {
        setPhase(Phase::Online);
    }
}

void IConsole::setPhase(Phase phase) noexcept
{
    if (phase == _phase) {
        return;
    }
    SMCP_CONS("[SMCP] IConsole::phase %s -> %s\n", cstr(_phase), cstr(phase));
    _phase = phase;
    onPhase(_phase);
}

void IConsole::enterFault() noexcept
{
    _listen.stop();
    _primary.close();
    setPhase(Phase::Fault);
}

void IConsole::startPrimary() noexcept
{
    if (_server_id == 0u) {
        return;
    }
    _primary.start(_server_id);
}

} // namespace smcp
