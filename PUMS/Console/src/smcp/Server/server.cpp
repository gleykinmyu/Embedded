/**
 * @file server.cpp
 * @brief IServer Telemetry + SessionConsole Select/Block/SetTarget.
 */

#include "smcp/Server/server.hpp"

#include <variant>

namespace smcp {

// =============================================================================
// detail
// =============================================================================

namespace detail {

void registerMech(IServer& server, IMech& mech) noexcept
{
    (void)server.storage().registerAt(mech.id(), &mech);
}

} // namespace detail

// =============================================================================
// IServer
// =============================================================================

IServer::IServer(ILink& link, ClockFn clock) noexcept
    : Node(link, clock)
{}

void IServer::pushTelemetry(uint8_t mech_id) noexcept
{
    const IMech* m = mech(mech_id);
    if (m == nullptr) {
        return;
    }

    msg::Telemetry tel{};
    tel.mech_id = mech_id;
    tel.holder_id = m->holder();
    tel.position_mm = m->position();
    tel.status = m->status();

    send(tel, msg::kBroadcastId);
}

// =============================================================================
// SessionConsole — ctor / RX
// =============================================================================

SessionConsole::SessionConsole(IServer& server) noexcept
    : Session(server)
    , _server(server)
{}

bool SessionConsole::onPacket(const msg::Packet& pkt) noexcept
{
    if (Session::onPacket(pkt)) {
        return true;
    }
    if (const auto* sel = std::get_if<msg::Select>(&pkt.body)) {
        handleMaskOp(MaskKind::Select, sel->action, sel->selection, pkt.pkt_id);
        return true;
    }
    if (const auto* blk = std::get_if<msg::Block>(&pkt.body)) {
        handleMaskOp(MaskKind::Block, blk->action, blk->selection, pkt.pkt_id);
        return true;
    }
    if (const auto* tgt = std::get_if<msg::SetTarget>(&pkt.body)) {
        onSetTarget(*tgt, pkt.pkt_id);
        return true;
    }
    return false;
}

// =============================================================================
// SessionConsole — общий пайплайн Select/Block
// =============================================================================

void SessionConsole::handleMaskOp(MaskKind kind,
                                  msg::Action action,
                                  Selection selection,
                                  uint8_t pkt_id) noexcept
{
    if (!msg::helpers::isConsoleId(peerId())) {
        return;
    }

    const uint8_t src = peerId();
    const std::size_t cap = _server.mechCapacity();
    const bool is_select = (kind == MaskKind::Select);

    MaskPlan plan{};
    for (uint8_t mid = 0; mid < cap; ++mid) {
        const bool in_mask = selection.contains(mid);
        IMech* m = _server.mech(mid);

        if (m == nullptr) {
            if (in_mask) {
                sendNack(pkt_id, msg::ErrorCode::MechNotFound);
                return;
            }
            continue;
        }

        const bool was = is_select ? m->isSelectedBy(src) : m->isBlocked();
        if (!in_mask && !was) {
            continue; /* нет вклада в next / changed */
        }

        if (is_select && in_mask) {
            const msg::ErrorCode guard = mechGuard(*m, src, /*must_own=*/false);
            if (guard != msg::ErrorCode::Ok) {
                sendNack(pkt_id, guard);
                return;
            }
        }

        plan.note(mid, was, in_mask, action);
    }

    const msg::ErrorCode policy = is_select ? _server.acceptSelect(src, plan.next)
                                            : _server.acceptBlock(src, plan.next);
    if (policy != msg::ErrorCode::Ok) {
        sendNack(pkt_id, policy);
        return;
    }

    if (is_select) {
        commitSelect(src, plan);
    } else {
        commitBlock(plan);
    }
    sendAck(pkt_id);
    pushTelemetryMask(plan.changed());
}

// =============================================================================
// SessionConsole — SetTarget
// =============================================================================

void SessionConsole::onSetTarget(const msg::SetTarget& body, uint8_t pkt_id) noexcept
{
    if (!msg::helpers::isConsoleId(peerId())) {
        return;
    }

    const uint8_t src = peerId();
    IMech* m = _server.mech(body.mech_id);
    if (m == nullptr) {
        sendNack(pkt_id, msg::ErrorCode::MechNotFound);
        return;
    }

    const msg::ErrorCode guard = mechGuard(*m, src, /*must_own=*/true);
    if (guard != msg::ErrorCode::Ok) {
        sendNack(pkt_id, guard);
        return;
    }

    const msg::ErrorCode policy = _server.acceptSetTarget(src, body.mech_id, body.target);
    if (policy != msg::ErrorCode::Ok) {
        sendNack(pkt_id, policy);
        return;
    }

    m->setTarget(body.target);
    sendAck(pkt_id);
    _server.pushTelemetry(body.mech_id);
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

msg::ErrorCode SessionConsole::mechGuard(const IMech& m, uint8_t src, bool must_own) noexcept
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
    if (!m.status().any(IMech::Status::Ready)) {
        return msg::ErrorCode::NotReady;
    }
    return msg::ErrorCode::Ok;
}

void SessionConsole::commitSelect(uint8_t src, const MaskPlan& plan) noexcept
{
    const std::size_t cap = _server.mechCapacity();
    const Selection changed = plan.changed();
    for (uint8_t mid = 0; mid < cap; ++mid) {
        if (!changed.contains(mid)) {
            continue;
        }
        IMech* m = _server.mech(mid);
        if (m == nullptr) {
            continue;
        }
        m->select(plan.take.contains(mid) ? src : kHolderNone);
    }
}

void SessionConsole::commitBlock(const MaskPlan& plan) noexcept
{
    const std::size_t cap = _server.mechCapacity();
    const Selection changed = plan.changed();
    for (uint8_t mid = 0; mid < cap; ++mid) {
        if (!changed.contains(mid)) {
            continue;
        }
        IMech* m = _server.mech(mid);
        if (m == nullptr) {
            continue;
        }
        m->block(plan.take.contains(mid));
    }
}

void SessionConsole::pushTelemetryMask(Selection mask) noexcept
{
    const std::size_t cap = _server.mechCapacity();
    for (uint8_t mid = 0; mid < cap; ++mid) {
        if (mask.contains(mid)) {
            _server.pushTelemetry(mid);
        }
    }
}

} // namespace smcp
