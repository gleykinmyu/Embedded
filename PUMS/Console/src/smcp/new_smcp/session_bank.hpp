/**
 * @file new_smcp/session_bank.hpp
 * @brief smcp::SessionBank<N> — пул Session[N] + LinkHbRoute для новых peer.
 *
 * Маршрут — член банка, но в списке узла: Base(node) цепляет банк, затем
 * конструктор маршрута цепляет его следом. Отдельный onPacket не нужен:
 * банк отдаёт кадр своей сессии, промах узел несёт следующему — маршруту.
 */

#pragma once

#include "comm/session.hpp"
#include "smcp/new_smcp/proto/link_hb_route.hpp"
#include "smcp/new_smcp/session.hpp"

namespace smcp {

template <uint8_t N>
class SessionBank : public comm::ProtoBank<Session, N, SessionSlots> {
    using Base = comm::ProtoBank<Session, N, SessionSlots>;

public:
    explicit SessionBank(comm::Node<Packet>& node) noexcept
        : Base(node)
        , _route(*this)
    {}

private:
    proto::LinkHbRoute _route;
};

} // namespace smcp
