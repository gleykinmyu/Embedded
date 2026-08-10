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

IServer::IServer(BIF::CAN::ICAN& can, uint8_t server_id) noexcept
    : Node(can, server_id)
{}

void IServer::setConsoleId(uint8_t console_id) noexcept
{
    if (Session* s = primarySession()) {
        s->setPeerId(console_id);
    }
}

uint8_t IServer::consoleId() const noexcept
{
    const Session* s = primarySession();
    return s != nullptr ? s->peerId() : uint8_t{0};
}

void IServer::startSession() noexcept
{
    if (Session* s = primarySession()) {
        s->start();
    }
}

void IServer::stopSession() noexcept
{
    if (Session* s = primarySession()) {
        s->stop();
    }
}

bool IServer::linkUp() const noexcept
{
    const Session* s = primarySession();
    return s != nullptr && s->isOpen();
}

void IServer::onPacket(const msg::Packet& pkt) noexcept
{
    if (const auto* sel = std::get_if<msg::Select>(&pkt.body)) {
        handleSelect(pkt.hdr, *sel, pkt.pkt_id);
    }
}

void IServer::handleSelect(const msg::Header& hdr, const msg::Select& body, uint8_t pkt_id) noexcept
{
    if (!msg::helpers::isConsoleId(hdr.src_id)) {
        return;
    }

    Session* sess = sessionByPeer(hdr.src_id);
    if (sess == nullptr) {
        return;
    }

    const auto action = body.action;
    const uint8_t src = hdr.src_id;
    const std::size_t cap = mechCapacity();

    /* --- Проверка + прогноз Selected (без изменений); commit ниже --- */
    Selection next_selected{};
    for (uint8_t mid = 0; mid < cap; ++mid) {
        const bool in_mask = body.selection.contains(mid);
        IMech* m = mech(mid);

        if (in_mask) {
            if (m == nullptr) {
                sess->sendNack(pkt_id, msg::ErrorCode::MechNotFound);
                return;
            }

            /* Чужой holder — отказ при любом action (Select/Set/Deselect). */
            if (m->isSelected() && !m->isSelectedBy(src)) {
                sess->sendNack(pkt_id, msg::ErrorCode::Busy);
                return;
            }

            /* Select/Set на свободную: Blocked / Ready — независимые флаги. Deselect — только holder. */
            if ((action == msg::Select::Action::Select || action == msg::Select::Action::Set)
                && !m->isSelectedBy(src)) {
                if (m->isBlocked()) {
                    // TODO: все ли консоли могут блокировать все механизмы (или только свои / политика сегмента)?
                    sess->sendNack(pkt_id, msg::ErrorCode::Safety);
                    return;
                }
                if (!m->status().any(IMech::Status::Ready)) {
                    sess->sendNack(pkt_id, msg::ErrorCode::NotReady);
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

        bool selected = m->isSelected();
        if (take) {
            selected = true;
        } else if (drop) {
            selected = false;
        }
        if (selected) {
            next_selected.add(mid);
        }
    }

    const msg::ErrorCode policy = acceptSelect(src, next_selected);
    if (policy != msg::ErrorCode::Ok) {
        sess->sendNack(pkt_id, policy);
        return;
    }

    /* --- Commit: take / drop в одном проходе --- */
    for (uint8_t mid = 0; mid < cap; ++mid) {
        IMech* m = mech(mid);
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
            (void)m->select(src);
            pushTelemetry(mid);
        } else if (drop) {
            (void)m->select(kHolderNone);
            pushTelemetry(mid);
        }
    }

    sess->sendAck(pkt_id);
}

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

void IServer::pushAck(uint8_t req_pkt_id) noexcept
{
    if (Session* s = primarySession()) {
        s->sendAck(req_pkt_id);
    }
}

void IServer::pushNack(uint8_t req_pkt_id, msg::ErrorCode code) noexcept
{
    if (Session* s = primarySession()) {
        s->sendNack(req_pkt_id, code);
    }
}

} // namespace smcp
