/**
 * @file server.cpp
 */

#include "smcp/Server/server.hpp"

#include <variant>

namespace smcp {

namespace detail {

void registerMech(IServer& server, IMech& mech) noexcept
{
    (void)server.storage().registerAt(mech.id(), &mech);
}

} // namespace detail

// --- ctor ---

IServer::IServer(ILink& link, ClockFn clock) noexcept
    : Node(link, clock)
{}

// --- исходящие PDU ---

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

// --- SessionConsole ---

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
        onSelect(*sel, pkt.pkt_id);
        return true;
    }
    return false;
}

void SessionConsole::onSelect(const msg::Select& body, uint8_t pkt_id) noexcept
{
    if (!msg::helpers::isConsoleId(peerId())) {
        return;
    }

    const auto action = body.action;
    const uint8_t src = peerId();
    const std::size_t cap = _server.mechCapacity();

    /* --- Проверка + прогноз Selected (без изменений); commit ниже --- */
    Selection next_selected{};
    for (uint8_t mid = 0; mid < cap; ++mid) {
        const bool in_mask = body.selection.contains(mid);
        IMech* m = _server.mech(mid);

        if (in_mask) {
            if (m == nullptr) {
                sendNack(pkt_id, msg::ErrorCode::MechNotFound);
                return;
            }

            /* Чужой holder — отказ при любом action (Select/Set/Deselect). */
            if (m->isSelected() && !m->isSelectedBy(src)) {
                sendNack(pkt_id, msg::ErrorCode::Busy);
                return;
            }

            /* Select/Set на свободную: Blocked / Ready — независимые флаги. Deselect — только holder. */
            if ((action == msg::Select::Action::Select || action == msg::Select::Action::Set)
                && !m->isSelectedBy(src)) {
                if (m->isBlocked()) {
                    // TODO: все ли консоли могут блокировать все механизмы (или только свои / политика сегмента)?
                    sendNack(pkt_id, msg::ErrorCode::Safety);
                    return;
                }
                if (!m->status().any(IMech::Status::Ready)) {
                    sendNack(pkt_id, msg::ErrorCode::NotReady);
                    return;
                }
            }
        }

        if (m == nullptr) {
            continue;
        }

        const bool was_ours = m->isSelectedBy(src);
        const bool take = (action == msg::Select::Action::Select
                           || action == msg::Select::Action::Set)
            && in_mask && !was_ours;
        /* Deselect: бит=1 и наше; Set: бит=0 и наше; чужое не снимаем. */
        const bool drop = was_ours
            && ((action == msg::Select::Action::Deselect && in_mask)
                || (action == msg::Select::Action::Set && !in_mask));

        /* Итог владения этой консоли (не сегментный Selected). */
        bool ours = was_ours;
        if (take) {
            ours = true;
        } else if (drop) {
            ours = false;
        }
        if (ours) {
            next_selected.add(mid);
        }
    }

    const msg::ErrorCode policy = _server.acceptSelect(src, next_selected);
    if (policy != msg::ErrorCode::Ok) {
        sendNack(pkt_id, policy);
        return;
    }

    /* --- Commit: take / drop в одном проходе --- */
    for (uint8_t mid = 0; mid < cap; ++mid) {
        IMech* m = _server.mech(mid);
        if (m == nullptr) {
            continue;
        }

        const bool in_mask = body.selection.contains(mid);
        const bool was_ours = m->isSelectedBy(src);

        const bool take = (action == msg::Select::Action::Select
                           || action == msg::Select::Action::Set)
            && in_mask && !was_ours;
        const bool drop = was_ours
            && ((action == msg::Select::Action::Deselect && in_mask)
                || (action == msg::Select::Action::Set && !in_mask));

        if (take) {
            // TODO: при проверках снаружи + атомарном commit — в каких случаях select() ещё
            // вернёт false/Busy? Кажется, bool у IMech::select уже бессмысленен.
            (void)m->select(src);
            _server.pushTelemetry(mid);
        } else if (drop) {
            // TODO: то же для select(kHolderNone) — отказ после внешних проверок?
            (void)m->select(kHolderNone);
            _server.pushTelemetry(mid);
        }
    }

    sendAck(pkt_id);
}

} // namespace smcp
