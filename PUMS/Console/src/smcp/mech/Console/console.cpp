/**
 * @file console.cpp
 * @brief IConsole: Phase lifecycle, master Session TX, Telemetry RX.
 */

#include "smcp/mech/Console/console.hpp"
#include "smcp/debug.hpp"

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

const char* IConsole::cstr(Phase phase) noexcept
{
    switch (phase) {
    case Phase::Idle: return "Idle";
    case Phase::Listen: return "Listen";
    case Phase::Ready: return "Ready";
    case Phase::Fault: return "Fault";
    }
    return "?";
}

IConsole::IConsole(ILink& link, ClockFn clock, Session& primary) noexcept
    : Node(link, clock)
    , _primary(primary)
{}

IConsole::MechReg* IConsole::segment(uint8_t server_id) noexcept
{
    if (server_id == CMech::kPrimaryServer) {
        return &storage();
    }
    uint8_t n = 0;
    uint8_t only = 0;
    for (uint8_t i = 0; i < kMaxStart; ++i) {
        if (_started[i] == 0) {
            continue;
        }
        ++n;
        only = _started[i];
        if (n > 1u) {
            break;
        }
    }
    if (n == 1u && server_id == only) {
        return &storage();
    }
    return nullptr;
}

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
        setPhase(Phase::Ready);
        break;

    case Phase::Ready:
        pumpReconnect();
        notifyLinkUp();
        break;
    }
}

bool IConsole::linkUp() const noexcept
{
    return _primary.isOpen();
}

bool IConsole::linkUp(uint8_t server_id) const noexcept
{
    const Session* const s = sessionTo(server_id);
    return s != nullptr && s->isOpen();
}

void IConsole::begin(uint8_t console_id) noexcept
{
    if (!msg::isConsoleId(console_id)) {
        return;
    }
    link().setNodeId(console_id);
    _listen.stop();
    closeAllSessions();
    if (getStatus() != Status::OK) {
        clearError();
    }
    _listen.start(clockMs(), msg::Heartbeat::kTimeoutMs);
    setPhase(Phase::Listen);
}

void IConsole::end() noexcept
{
    _listen.stop();
    closeAllSessions();
    if (getStatus() != Status::OK) {
        clearError();
    }
    setPhase(Phase::Idle);
}

void IConsole::start(uint8_t server_id) noexcept
{
    if (_phase != Phase::Ready) {
        return;
    }
    if (!msg::isServerId(server_id)) {
        return;
    }

    Session* existing = sessionByPeer(server_id);
    if (existing != nullptr) {
        (void)rememberStart(server_id);
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
    if (!rememberStart(server_id)) {
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

    const uint8_t i = startSlot(peer);
    if (i < kMaxStart && _link_up[i]) {
        _link_up[i] = false;
        onLink(peer, false);
    }
    if (s != nullptr) {
        s->close();
    }
    forgetStart(peer);
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
    }
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
    if (session == nullptr) {
        return;
    }
    const uint8_t peer = session->peerId();
    const uint8_t i = startSlot(peer);
    if (i < kMaxStart && _link_up[i]) {
        _link_up[i] = false;
        onLink(peer, false);
    }
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
    closeAllSessions();
    setPhase(Phase::Fault);
}

void IConsole::closeAllSessions() noexcept
{
    for (uint8_t i = 0; i < kMaxStart; ++i) {
        if (_started[i] == 0) {
            continue;
        }
        if (_link_up[i]) {
            _link_up[i] = false;
            onLink(_started[i], false);
        }
        _started[i] = 0;
    }

    SessionReg& reg = sessions();
    const uint8_t end = reg.endId();
    for (uint8_t sid = reg.firstId(); sid < end; ++sid) {
        Session* const s = reg.get(sid);
        if (s != nullptr) {
            s->close();
        }
    }
}

void IConsole::pumpReconnect() noexcept
{
    for (uint8_t i = 0; i < kMaxStart; ++i) {
        const uint8_t peer = _started[i];
        if (peer == 0) {
            continue;
        }
        Session* existing = sessionByPeer(peer);
        if (existing != nullptr) {
            if (existing->getStatus() == Session::Status::Idle) {
                existing->start(peer);
            }
            continue;
        }
        Session* const slot = idleSession();
        if (slot == nullptr) {
            continue;
        }
        slot->start(peer);
    }
}

void IConsole::notifyLinkUp() noexcept
{
    SessionReg& reg = sessions();
    const uint8_t end = reg.endId();
    for (uint8_t sid = reg.firstId(); sid < end; ++sid) {
        Session* const s = reg.get(sid);
        if (s == nullptr || !s->isOpen()) {
            continue;
        }
        const uint8_t peer = s->peerId();
        const uint8_t i = startSlot(peer);
        if (i >= kMaxStart || _link_up[i]) {
            continue;
        }
        _link_up[i] = true;
        onLink(peer, true);
    }
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

uint8_t IConsole::startSlot(uint8_t server_id) const noexcept
{
    for (uint8_t i = 0; i < kMaxStart; ++i) {
        if (_started[i] == server_id) {
            return i;
        }
    }
    return kMaxStart;
}

bool IConsole::rememberStart(uint8_t server_id) noexcept
{
    if (startSlot(server_id) < kMaxStart) {
        return true;
    }
    for (uint8_t i = 0; i < kMaxStart; ++i) {
        if (_started[i] == 0) {
            _started[i] = server_id;
            _link_up[i] = false;
            return true;
        }
    }
    return false;
}

void IConsole::forgetStart(uint8_t server_id) noexcept
{
    const uint8_t i = startSlot(server_id);
    if (i >= kMaxStart) {
        return;
    }
    _started[i] = 0;
    _link_up[i] = false;
}

} // namespace smcp
