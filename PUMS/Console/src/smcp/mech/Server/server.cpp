/**
 * @file server.cpp
 * @brief IServer Telemetry + SessionConsole Select/Block/SetTarget.
 */

#include "smcp/mech/Server/server.hpp"

namespace smcp {

// =============================================================================
// detail
// =============================================================================

namespace detail {

void registerMech(IServer& server, IAxis& mech) noexcept
{
    const MISC::RegStatus st = server.storage().registerAt(mech.id(), &mech);
    if (st != MISC::RegStatus::Ok) {
        server.setStatus(Node::Status::RegisterFailed);
        return;
    }
    server._mechRegistered.add(mech.id());
}

} // namespace detail

// =============================================================================
// IServer
// =============================================================================

IServer::IServer(ILink& link, ClockFn clock) noexcept
    : Node(link, clock)
{}

void IServer::begin(uint8_t server_id) noexcept
{
    if (!msg::isServerId(server_id)) {
        return;
    }
    Node::begin(server_id);
}

void IServer::pushTelemetry(uint8_t axis_id) noexcept
{
    const IAxis* m = mech(axis_id);
    if (m == nullptr) {
        return;
    }

    msg::Telemetry tel{};
    tel.axis_id = axis_id;
    tel.holder_id = m->holder();
    tel.position = m->position();
    tel.status = m->status();
    tel.reserved = 0;

    send(tel, Packet::kBroadcastId);
}

void IServer::dropPeerSelect(Session* session, bool telemetry) noexcept
{
    if (session == nullptr) {
        return;
    }
    const uint8_t src = session->peerId();
    if (!msg::isConsoleId(src)) {
        return;
    }

    const uint8_t cap = mechCapacity();
    for (uint8_t mid = 0; mid < cap; ++mid) {
        IAxis* m = mech(mid);
        if (m == nullptr || !m->isSelectedBy(src)) {
            continue;
        }
        m->select(kHolderNone);
        if (telemetry) {
            pushTelemetry(mid);
        }
    }
}

void IServer::onFault(Session* session, Fault reason) noexcept
{
    Node::onFault(session, reason);
    if (session != nullptr || reason != Fault::IdConflict) {
        return;
    }

    const uint8_t cap = mechCapacity();
    for (uint8_t mid = 0; mid < cap; ++mid) {
        IAxis* m = mech(mid);
        if (m == nullptr || m->holder() == kHolderNone) {
            continue;
        }
        m->select(kHolderNone);
    }
}

// =============================================================================
// SessionConsole — ctor / RX
// =============================================================================

SessionConsole::SessionConsole(IServer& server) noexcept
    : Session(server)
    , _server(server)
{}

void SessionConsole::onFault(Fault reason) noexcept
{
    _server.dropPeerSelect(this, /*telemetry=*/true);
    Session::onFault(reason);
}

bool SessionConsole::onPacket(const Packet& pkt) noexcept
{
    if (Session::onPacket(pkt)) {
        return true;
    }
    if (!isOpen()) {
        return false;
    }
    SMCP_IF_MSG(msg::Select) {
        handleMaskOp(MaskKind::Select, body.action, body.selection, pkt.txn_id);
    } else SMCP_IF_MSG(msg::Block) {
        handleMaskOp(MaskKind::Block, body.action, body.selection, pkt.txn_id);
    } else SMCP_IF_MSG(msg::SetTarget) {
        onSetTarget(body, pkt.txn_id);
    } else SMCP_IF_MSG(msg::GetTelemetry) {
        onGetTelemetry(body, pkt.txn_id);
    } else SMCP_IF_MSG(msg::ResetFault) {
        onResetFault(body, pkt.txn_id);
    } else {
        return false;
    }
    return true;
}

// =============================================================================
// SessionConsole — общий пайплайн Select/Block
// =============================================================================

void SessionConsole::handleMaskOp(MaskKind kind,
                                  msg::Action action,
                                  Selection selection,
                                  uint8_t txn_id) noexcept
{
    if (!msg::isConsoleId(peerId())) {
        return;
    }

    const uint8_t src = peerId();
    const uint8_t cap = _server.mechCapacity();
    const bool is_select = (kind == MaskKind::Select);

    if (checkMechId(selection, txn_id)) {
        return;
    }

    MaskPlan plan{};
    for (uint8_t mid = 0; mid < cap; ++mid) {
        const bool in_mask = selection.contains(mid);
        IAxis* m = _server.mech(mid);

        if (m == nullptr) {
            continue;
        }

        const bool was = is_select ? m->isSelectedBy(src) : m->isBlocked();
        if (!in_mask && !was) {
            continue; /* нет вклада в next / changed */
        }

        if (is_select && in_mask) {
            const msg::ErrorCode guard = mechGuard(*m, src, /*must_own=*/false);
            if (guard != msg::ErrorCode::Ok) {
                sendNack(txn_id, guard, Selection::of(mid));
                return;
            }
        }

        plan.note(mid, was, in_mask, action);
    }

    const msg::ErrorCode policy = is_select ? _server.acceptSelect(src, plan.next)
                                            : _server.acceptBlock(src, plan.next);
    if (policy != msg::ErrorCode::Ok) {
        sendNack(txn_id, policy);
        return;
    }

    if (is_select) {
        commitSelect(src, plan);
    } else {
        commitBlock(plan);
    }
    sendAck(txn_id);
    pushTelemetryMask(plan.changed());
}

// =============================================================================
// SessionConsole — SetTarget
// =============================================================================

void SessionConsole::onSetTarget(const msg::SetTarget& body, uint8_t txn_id) noexcept
{
    if (!msg::isConsoleId(peerId())) {
        return;
    }

    const uint8_t src = peerId();
    const uint8_t cap = _server.mechCapacity();

    Selection want{};
    if (body.all_selected) {
        for (uint8_t mid = 0; mid < cap; ++mid) {
            IAxis* m = _server.mech(mid);
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
        IAxis* m = _server.mech(mid);
        if (m == nullptr) {
            sendNack(txn_id, msg::ErrorCode::MechNotFound, Selection::of(mid));
            return;
        }
        const msg::ErrorCode guard = mechGuard(*m, src, /*must_own=*/true);
        if (guard != msg::ErrorCode::Ok) {
            sendNack(txn_id, guard, Selection::of(mid));
            return;
        }
        const msg::ErrorCode policy = _server.acceptSetTarget(src, mid, body.target);
        if (policy != msg::ErrorCode::Ok) {
            sendNack(txn_id, policy, Selection::of(mid));
            return;
        }
    }

    for (uint8_t mid = 0; mid < cap; ++mid) {
        if (!want.contains(mid)) {
            continue;
        }
        IAxis* m = _server.mech(mid);
        if (m != nullptr) {
            m->setTarget(body.target);
        }
    }

    sendAck(txn_id);
    pushTelemetryMask(want);
}

void SessionConsole::onResetFault(const msg::ResetFault& body, uint8_t txn_id) noexcept
{
    if (!msg::isConsoleId(peerId())) {
        return;
    }

    Selection want = body.selection;
    if (want.empty()) {
        _server.resetGlobalFault();
        sendAck(txn_id);
        return;
    }
    if (checkMechId(want, txn_id)) {
        return;
    }

    const uint8_t cap = _server.mechCapacity();
    for (uint8_t mid = 0; mid < cap; ++mid) {
        if (!want.contains(mid)) {
            continue;
        }
        IAxis* m = _server.mech(mid);
        if (m != nullptr) {
            m->resetFault();
        }
    }

    sendAck(txn_id);
    pushTelemetryMask(want);
}

void SessionConsole::onGetTelemetry(const msg::GetTelemetry& body, uint8_t txn_id) noexcept
{
    if (!msg::isConsoleId(peerId())) {
        return;
    }

    Selection want = body.selection;
    if (want.empty()) {
        want = _server.mechRegistered();
    } else if (checkMechId(want, txn_id)) {
        return;
    }

    sendAck(txn_id);
    pushTelemetryMask(want);
}

// =============================================================================
// SessionConsole — MaskPlan / guards / commit
// =============================================================================

void SessionConsole::MaskPlan::note(uint8_t mid, bool was, bool in_mask, msg::Action action) noexcept
{
    const bool want = (action == msg::Action::Set)
        ? in_mask
        : (in_mask ? (action == msg::Action::Add) : was);

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

msg::ErrorCode SessionConsole::mechGuard(const IAxis& m, uint8_t src, bool must_own) noexcept
{
    if (must_own) {
        if (!m.isSelectedBy(src)) {
            return msg::ErrorCode::Busy; /* свободная или чужая */
        }
    } else if (m.isSelected() && !m.isSelectedBy(src)) {
        return msg::ErrorCode::Busy; /* чужой holder */
    } else if (m.isSelectedBy(src)) {
        return msg::ErrorCode::Ok; /* своя — drive не нужен (Select) */
    }

    /* SetTarget (уже наша) или Select take свободной — Blocked / Ready. */
    if (m.isBlocked()) {
        return msg::ErrorCode::Safety;
    }
    if (!m.status().any(IAxis::Status::Ready)) {
        return msg::ErrorCode::NotReady;
    }
    return msg::ErrorCode::Ok;
}

bool SessionConsole::checkMechId(Selection selection, uint8_t txn_id) noexcept
{
    const Selection missing = selection & ~_server.mechRegistered();
    if (missing.empty()) {
        return false;
    }
    sendNack(txn_id, msg::ErrorCode::MechNotFound, missing);
    return true;
}

void SessionConsole::commitSelect(uint8_t src, const MaskPlan& plan) noexcept
{
    const uint8_t cap = _server.mechCapacity();

    /* Сначала drop, потом take. */
    for (uint8_t mid = 0; mid < cap; ++mid) {
        if (!plan.drop.contains(mid)) {
            continue;
        }
        IAxis* m = _server.mech(mid);
        if (m != nullptr) {
            m->select(kHolderNone);
        }
    }
    for (uint8_t mid = 0; mid < cap; ++mid) {
        if (!plan.take.contains(mid)) {
            continue;
        }
        IAxis* m = _server.mech(mid);
        if (m != nullptr) {
            m->select(src);
        }
    }
}

void SessionConsole::commitBlock(const MaskPlan& plan) noexcept
{
    const uint8_t cap = _server.mechCapacity();
    const Selection changed = plan.changed();
    for (uint8_t mid = 0; mid < cap; ++mid) {
        if (!changed.contains(mid)) {
            continue;
        }
        IAxis* m = _server.mech(mid);
        if (m == nullptr) {
            continue;
        }
        m->block(plan.take.contains(mid));
    }
}

void SessionConsole::pushTelemetryMask(Selection mask) noexcept
{
    const uint8_t cap = _server.mechCapacity();
    for (uint8_t mid = 0; mid < cap; ++mid) {
        if (mask.contains(mid)) {
            _server.pushTelemetry(mid);
        }
    }
}

} // namespace smcp
