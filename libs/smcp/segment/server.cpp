/**
 * @file server.cpp
 * @brief IServer Telemetry + Session Select/Block/SetTarget/AxisProfile Get-Set.
 */

#include "segment/server.hpp"

#include <cstring>

namespace smcp::segment {

namespace server {

// =============================================================================
// detail
// =============================================================================

namespace detail {

void registerAxis(IServer& server, Axis& axis) noexcept
{
    const MISC::RegStatus st = server.storage().registerAt(axis.axisId(), &axis);
    if (st != MISC::RegStatus::Ok) {
        server.setError(Node::Error::RegisterFailed);
        return;
    }
    server._axisRegistered.selection.add(axis.axisId());
}

} // namespace detail

// =============================================================================
// Axis
// =============================================================================

Axis::Axis(IServer& owner, idx_t axis_id) noexcept
    : _axis_id(axis_id)
{
    profile.axis_id = axis_id;
    detail::registerAxis(owner, *this);
    setStatus(axis::Status::Ready, true);
}

bool Axis::select(uint8_t console_id) noexcept
{
    _holder = console_id;
    return true;
}

bool Axis::block(bool blocked) noexcept
{
    if (blocked) {
        if (isSelected()) {
            (void)select(kHolderNone);
        }
    }
    setStatus(axis::Status::Blocked, blocked);
    return true;
}

} // namespace server

// =============================================================================
// IServer
// =============================================================================

IServer::IServer(ILink& link, ClockFn clock) noexcept
    : Node(link, clock)
{}

void IServer::begin(uint8_t server_id) noexcept
{
    if (!segment::isServerId(server_id)) {
        return;
    }
    Node::begin(server_id);
}

void IServer::sendTelemetry(server::idx_t axis_id) noexcept
{
    const server::Axis* m = axis(axis_id);
    if (m == nullptr) {
        return;
    }

    send(segment::Telemetry::from(axis_id, *m), Packet::kBroadcastId);
}

bool IServer::lockService(uint8_t console_id) noexcept
{
    if (_serviceHolder == 0u) {
        return true;
    }
    if (_serviceHolder != console_id) {
        return false;
    }
    _serviceHolder = 0u;
    _serviceUnlockAtMs = 0u;
    return true;
}

bool IServer::unlockService(uint8_t console_id) noexcept
{
    if (console_id == 0u) {
        return false;
    }
    if (_serviceHolder != 0u && _serviceHolder != console_id) {
        return false;
    }
    _serviceHolder = console_id;
    _serviceUnlockAtMs = clockMs();
    return true;
}

void IServer::serviceTick() noexcept
{
    if (_serviceHolder == 0u) {
        return;
    }
    if (static_cast<uint32_t>(clockMs() - _serviceUnlockAtMs) < segment::ServiceAccess::kHoldMs) {
        return;
    }
    _serviceHolder = 0u;
    _serviceUnlockAtMs = 0u;
}

void IServer::dropPeerSelect(server::Session* session, bool telemetry) noexcept
{
    if (session == nullptr) {
        return;
    }
    const uint8_t src = session->peerId();
    if (!segment::isConsoleId(src)) {
        return;
    }

    const uint8_t cap = axisCapacity();
    for (uint8_t mid = 0; mid < cap; ++mid) {
        server::Axis* m = axis(mid);
        if (m == nullptr || !m->isSelectedBy(src)) {
            continue;
        }
        (void)m->select(kHolderNone);
        if (telemetry) {
            sendTelemetry(mid);
        }
    }
}

void IServer::onLink(::smcp::Session* session, Link link) noexcept
{
    Node::onLink(session, link);
    if (session == nullptr || link == Link::Up) {
        return;
    }
    auto* const ss = static_cast<server::Session*>(session);
    (void)lockService(ss->peerId());
    const bool lost = (link == Link::HbLost || link == Link::PeerDown);
    dropPeerSelect(ss, /*telemetry=*/lost);
}

void IServer::onError(Error error) noexcept
{
    Node::onError(error);
    if (error != Error::IdConflict) {
        return;
    }
    /* Сессии уже close(Down); хвосты holder без живой сессии. */
    const uint8_t cap = axisCapacity();
    for (uint8_t mid = 0; mid < cap; ++mid) {
        server::Axis* m = axis(mid);
        if (m == nullptr || m->holder() == kHolderNone) {
            continue;
        }
        (void)m->select(kHolderNone);
    }
}

namespace server {

// =============================================================================
// Session — ctor / RX
// =============================================================================

Session::Session(IServer& server) noexcept
    : ::smcp::Session(server)
    , _server(server)
{}

void Session::tick() noexcept
{
    ::smcp::Session::tick();
    if (isServiceUnlocked()) {
        _server.serviceTick();
    }
}

bool Session::onPacket(const Packet& pkt) noexcept
{
    if (::smcp::Session::onPacket(pkt)) {
        return true;
    }
    if (!isOpen()) {
        return false;
    }
    SMCP_IF_MSG(segment::Select) {
        handleMaskOp(MaskKind::Select, body.action, body.selection, pkt.txn_id);
    } else SMCP_IF_MSG(segment::Block) {
        handleMaskOp(MaskKind::Block, body.action, body.selection, pkt.txn_id);
    } else SMCP_IF_MSG(segment::SetTarget) {
        onSetTarget(body, pkt.txn_id);
    } else SMCP_IF_MSG(segment::GetTelemetry) {
        onGetTelemetry(body, pkt.txn_id);
    } else SMCP_IF_MSG(segment::ResetFault) {
        onResetFault(body, pkt.txn_id);
    } else SMCP_IF_MSG(segment::ServiceAccess) {
        onServiceAccess(body, pkt.txn_id);
    } else {
        return false;
    }
    return true;
}

// =============================================================================
// Session — Get AxisRegistered / Get-Set AxisProfile + ServiceAccess
// =============================================================================

bool Session::checkAxisIndex(uint8_t index, uint8_t txn_id, bool allow_all) noexcept
{
    if (allow_all && index == segment::ObjTag::kIndexAll) {
        return true;
    }
    if (index > axis::kAxisIdMax) {
        sendNack(txn_id, segment::Error::AxisNotFound);
        return false;
    }
    return true;
}

void Session::onServiceAccess(const segment::ServiceAccess& body, uint8_t txn_id) noexcept
{
    const uint8_t src = peerId();
    if (!body.unlock) {
        if (!_server.lockService(src)) {
            sendNack(txn_id, segment::Error::Busy);
            return;
        }
        sendAck(txn_id);
        return;
    }
    const uint32_t want = _server.servicePinHash();
    if (want == 0u || body.pin_hash != want) {
        sendNack(txn_id, segment::Error::Safety);
        return;
    }
    if (!_server.unlockService(src)) {
        sendNack(txn_id, segment::Error::Busy);
        return;
    }
    sendAck(txn_id);
}

void Session::onGet(const session::Get& body, uint8_t txn_id) noexcept
{
    if (body.tag == segment::ObjTag::AxisRegistered) {
        onGetAxisRegistered(txn_id);
        return;
    }
    if (body.tag == segment::ObjTag::AxisProfile) {
        onGetAxisProfile(body.index, txn_id);
        return;
    }
    sendNack(txn_id, session::Error::NotSupported);
}

void Session::onGetAxisRegistered(uint8_t txn_id) noexcept
{
    AxisRegisteredInfo info = _server.axisRegistered();
    const IBuf buf{reinterpret_cast<uint8_t*>(&info),
                   sizeof(AxisRegisteredInfo)};
    /* inline Data: pack копирует payload до возврата из send. */
    if (!send(segment::ObjTag::AxisRegistered, buf, txn_id,
              segment::ObjTag::kIndexAll)) {
        sendNack(txn_id, session::Error::Busy);
    }
}

void Session::onGetAxisProfile(uint8_t index, uint8_t txn_id) noexcept
{
    if (index == segment::ObjTag::kIndexAll) {
        sendNack(txn_id, session::Error::NotSupported);
        return;
    }
    if (!checkAxisIndex(index, txn_id, /*allow_all=*/false)) {
        return;
    }

    Axis* a = _server.axis(index);
    if (a == nullptr) {
        sendNack(txn_id, segment::Error::AxisNotFound);
        return;
    }
    IBuf buf{reinterpret_cast<uint8_t*>(&a->profile),
             static_cast<uint16_t>(sizeof(axis::Profile))};
    if (!send(segment::ObjTag::AxisProfile, buf, txn_id, index)) {
        sendNack(txn_id, session::Error::Busy);
    }
}

bool Session::onSetBuffer(uint8_t tag, uint8_t index, uint8_t txn_id, uint16_t size,
                                 IBuf& buf) noexcept
{
    (void)txn_id;
    if (tag != segment::ObjTag::AxisProfile) {
        return false;
    }
    /* Отказ → TaggedOps Nack NotSupported; PIN/id — до приёма тела. */
    if (!isServiceUnlocked()) {
        return false;
    }
    if (index == segment::ObjTag::kIndexAll || index > axis::kAxisIdMax) {
        return false;
    }
    if (_server.axis(index) == nullptr) {
        return false;
    }
    if (size != sizeof(axis::Profile)) {
        return false;
    }
    buf = {reinterpret_cast<uint8_t*>(&_server._profileScratch), size};
    return true;
}

session::vError Session::onSet(uint8_t tag, uint8_t index, const IBuf& buf) noexcept
{
    if (tag != segment::ObjTag::AxisProfile) {
        return session::Error::NotSupported;
    }
    return onSetAxisProfile(index, buf);
}

segment::vError Session::onSetAxisProfile(uint8_t index, const IBuf& buf) noexcept
{
    /* Транспорт уже Ok; None → Ack, иначе Nack(error). */
    if (!isServiceUnlocked()) {
        return segment::Error::Safety;
    }
    if (index == segment::ObjTag::kIndexAll || index > axis::kAxisIdMax) {
        return segment::Error::AxisNotFound;
    }
    if (buf.size != sizeof(axis::Profile) || buf.data == nullptr) {
        return session::Error::NotSupported;
    }
    Axis* a = _server.axis(index);
    if (a == nullptr) {
        return segment::Error::AxisNotFound;
    }
    std::memcpy(&a->profile, buf.data, sizeof(axis::Profile));
    a->profile.axis_id = index;
    _server.bumpAxisRegisteredRevision();
    return session::Error::None;
}

// =============================================================================
// Session — общий пайплайн Select/Block
// =============================================================================

void Session::handleMaskOp(MaskKind kind,
                                  segment::Action action,
                                  segment::Selection selection,
                                  uint8_t txn_id) noexcept
{
    if (!segment::isConsoleId(peerId())) {
        return;
    }

    const uint8_t src = peerId();
    const uint8_t cap = _server.axisCapacity();
    const bool is_select = (kind == MaskKind::Select);

    if (checkMechId(selection, txn_id)) {
        return;
    }

    MaskPlan plan{};
    for (uint8_t mid = 0; mid < cap; ++mid) {
        const bool in_mask = selection.contains(mid);
        Axis* m = _server.axis(mid);

        if (m == nullptr) {
            continue;
        }

        const bool was = is_select ? m->isSelectedBy(src) : m->isBlocked();
        if (!in_mask && !was) {
            continue; /* нет вклада в next / changed */
        }

        if (is_select && in_mask) {
            const segment::vError guard = mechGuard(*m, src, /*must_own=*/false);
            if (guard != session::Error::None) {
                sendNack(txn_id, guard, segment::Selection::of(mid));
                return;
            }
        }

        plan.note(mid, was, in_mask, action);
    }

    const segment::vError policy = is_select ? _server.acceptSelect(src, plan.next)
                                             : _server.acceptBlock(src, plan.next);
    if (policy != session::Error::None) {
        sendNack(txn_id, policy);
        return;
    }

    if (is_select) {
        commitSelect(src, plan);
    } else {
        commitBlock(plan);
    }
    sendAck(txn_id);
    sendTelemetryMask(plan.changed());
}

// =============================================================================
// Session — SetTarget
// =============================================================================

void Session::onSetTarget(const segment::SetTarget& body, uint8_t txn_id) noexcept
{
    if (!segment::isConsoleId(peerId())) {
        return;
    }

    const uint8_t src = peerId();
    const uint8_t cap = _server.axisCapacity();

    segment::Selection want{};
    if (body.all_selected) {
        for (uint8_t mid = 0; mid < cap; ++mid) {
            Axis* m = _server.axis(mid);
            if (m != nullptr && m->isSelectedBy(src)) {
                want.add(mid);
            }
        }
    } else {
        want.add(body.axis_id);
    }

    if (want.empty()) {
        sendAck(txn_id);
        return;
    }

    for (uint8_t mid = 0; mid < cap; ++mid) {
        if (!want.contains(mid)) {
            continue;
        }
        Axis* m = _server.axis(mid);
        if (m == nullptr) {
            sendNack(txn_id, segment::Error::AxisNotFound, segment::Selection::of(mid));
            return;
        }
        const segment::vError guard = mechGuard(*m, src, /*must_own=*/true);
        if (guard != session::Error::None) {
            sendNack(txn_id, guard, segment::Selection::of(mid));
            return;
        }
        const segment::vError policy = _server.acceptSetTarget(src, mid, body.target);
        if (policy != session::Error::None) {
            sendNack(txn_id, policy, segment::Selection::of(mid));
            return;
        }
    }

    for (uint8_t mid = 0; mid < cap; ++mid) {
        if (!want.contains(mid)) {
            continue;
        }
        Axis* m = _server.axis(mid);
        if (m != nullptr) {
            (void)m->setTarget(body.target);
        }
    }

    sendAck(txn_id);
    sendTelemetryMask(want);
}

void Session::onResetFault(const segment::ResetFault& body, uint8_t txn_id) noexcept
{
    if (!segment::isConsoleId(peerId())) {
        return;
    }

    segment::Selection want = body.selection;
    if (want.empty()) {
        _server.resetGlobalFault();
        sendAck(txn_id);
        return;
    }
    if (checkMechId(want, txn_id)) {
        return;
    }

    const uint8_t cap = _server.axisCapacity();
    for (uint8_t mid = 0; mid < cap; ++mid) {
        if (!want.contains(mid)) {
            continue;
        }
        Axis* m = _server.axis(mid);
        if (m != nullptr) {
            (void)m->resetFault();
        }
    }

    sendAck(txn_id);
    sendTelemetryMask(want);
}

void Session::onGetTelemetry(const segment::GetTelemetry& body, uint8_t txn_id) noexcept
{
    if (!segment::isConsoleId(peerId())) {
        return;
    }

    segment::Selection want = body.selection;
    if (want.empty()) {
        want = _server.axisRegistered().selection;
    } else if (checkMechId(want, txn_id)) {
        return;
    }

    sendAck(txn_id);
    sendTelemetryMask(want);
}

// =============================================================================
// Session — MaskPlan / guards / commit
// =============================================================================

void Session::MaskPlan::note(uint8_t mid, bool was, bool in_mask, segment::Action action) noexcept
{
    const bool want = (action == segment::Action::Set)
        ? in_mask
        : (in_mask ? (action == segment::Action::Add) : was);

    if (want != was) {
        if (want) {
            take.add(mid);
        } else {
            drop.add(mid);
        }
    }
    if (want) {
        next.add(mid);
    }
}

segment::vError Session::mechGuard(const Axis& m, uint8_t src, bool must_own) noexcept
{
    if (must_own) {
        if (!m.isSelectedBy(src)) {
            return segment::Error::Busy; /* свободная или чужая */
        }
    } else if (m.isSelected() && !m.isSelectedBy(src)) {
        return segment::Error::Busy; /* чужой holder */
    } else if (m.isSelectedBy(src)) {
        return session::Error::None; /* своя — drive не нужен (Select) */
    }

    /* SetTarget (уже наша) или Select take свободной — Blocked / Ready. */
    if (m.isBlocked()) {
        return segment::Error::Safety;
    }
    if (!m.status().any(axis::Status::Ready)) {
        return segment::Error::NotReady;
    }
    return session::Error::None;
}

bool Session::checkMechId(segment::Selection selection, uint8_t txn_id) noexcept
{
    const segment::Selection missing =
        selection & ~_server.axisRegistered().selection;
    if (missing.empty()) {
        return false;
    }
    sendNack(txn_id, segment::Error::AxisNotFound, missing);
    return true;
}

void Session::commitSelect(uint8_t src, const MaskPlan& plan) noexcept
{
    const uint8_t cap = _server.axisCapacity();

    /* Сначала drop, потом take. */
    for (uint8_t mid = 0; mid < cap; ++mid) {
        if (!plan.drop.contains(mid)) {
            continue;
        }
        Axis* m = _server.axis(mid);
        if (m != nullptr) {
            (void)m->select(kHolderNone);
        }
    }
    for (uint8_t mid = 0; mid < cap; ++mid) {
        if (!plan.take.contains(mid)) {
            continue;
        }
        Axis* m = _server.axis(mid);
        if (m != nullptr) {
            (void)m->select(src);
        }
    }
}

void Session::commitBlock(const MaskPlan& plan) noexcept
{
    const uint8_t cap = _server.axisCapacity();
    const segment::Selection changed = plan.changed();
    for (uint8_t mid = 0; mid < cap; ++mid) {
        if (!changed.contains(mid)) {
            continue;
        }
        Axis* m = _server.axis(mid);
        if (m == nullptr) {
            continue;
        }
        (void)m->block(plan.take.contains(mid));
    }
}

void Session::sendTelemetryMask(segment::Selection mask) noexcept
{
    const uint8_t cap = _server.axisCapacity();
    for (uint8_t mid = 0; mid < cap; ++mid) {
        if (mask.contains(mid)) {
            _server.sendTelemetry(mid);
        }
    }
}

} // namespace server
} // namespace smcp::segment
