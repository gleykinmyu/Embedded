/**
 * @file server.hpp
 * @brief IServer + Server<N>: Select→Ack + Telemetry(on change).
 *
 * Наследует Node. Session* — в registry; объекты Session владеет leaf (напр. MServer).
 * Сервер: много сессий (до kMaxConsoles); консоль — обычно одна.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "ican.hpp"
#include "obj_registry.hpp"
#include "smcp/Console/group.hpp"
#include "smcp/mech.hpp"
#include "smcp/transport/message.hpp"
#include "smcp/transport/node.hpp"
#include "smcp/transport/session.hpp"

namespace smcp {

class IServer;

namespace detail {
void registerMech(IServer& server, IMech& mech) noexcept;
} // namespace detail

class IServer : public Node {
public:
    virtual ~IServer() = default;

    [[nodiscard]] std::size_t mechCapacity() const noexcept
    {
        return const_cast<IServer*>(this)->storage().capacity();
    }

    [[nodiscard]] IMech* mech(uint8_t id) noexcept { return storage().get(id); }
    [[nodiscard]] const IMech* mech(uint8_t id) const noexcept
    {
        return const_cast<IServer*>(this)->storage().get(id);
    }

    /** Слот 0 — первая сессия; на сервере смотри sessionByPeer / SessionBank. */
    [[nodiscard]] Session* primarySession() noexcept { return session(0); }
    [[nodiscard]] const Session* primarySession() const noexcept { return session(0); }

    void setConsoleId(uint8_t console_id) noexcept;
    [[nodiscard]] uint8_t consoleId() const noexcept;

    void startSession() noexcept;
    void stopSession() noexcept;

    [[nodiscard]] bool linkUp() const noexcept;

    void pushTelemetry(uint8_t mech_id) noexcept;
    void pushAck(uint8_t req_pkt_id) noexcept;
    void pushNack(uint8_t req_pkt_id, msg::ErrorCode code) noexcept;

protected:
    friend void detail::registerMech(IServer& server, IMech& mech) noexcept;

    explicit IServer(BIF::CAN::ICAN& can, uint8_t server_id) noexcept;

    [[nodiscard]] virtual MISC::ObjRegistry<IMech, uint8_t>& storage() noexcept = 0;

    void onPacket(const msg::Packet& pkt) noexcept override;
    void handleSelect(const msg::Header& hdr, const msg::Select& body, uint8_t pkt_id) noexcept;

    /**
     * Политика сегмента: можно ли перейти к маске Selected после запроса.
     * @a selected — итоговая маска Selected на сегменте (после take/drop, без commit).
     * @return ErrorCode::Ok или причина отказа (SelectLimit / …).
     */
    [[nodiscard]] virtual msg::ErrorCode acceptSelect(uint8_t console_id,
                                                      Selection selected) const noexcept
    {
        (void)console_id;
        (void)selected;
        return msg::ErrorCode::Ok;
    }
};

template <std::size_t MaxMechs, std::size_t MaxSessions = msg::kMaxConsoles>
class Server : public IServer {
public:
    static constexpr std::size_t kMechCount = MaxMechs;
    static constexpr std::size_t kSessionCount = MaxSessions;

    explicit Server(BIF::CAN::ICAN& can, uint8_t server_id = msg::kServerIdMin) noexcept
        : IServer(can, server_id)
    {}

protected:
    [[nodiscard]] MISC::ObjRegistry<IMech, uint8_t>& storage() noexcept override { return _mechs; }
    [[nodiscard]] MISC::ObjRegistry<Session, uint8_t>& sessions() noexcept override
    {
        return _sessions;
    }

    MISC::ObjStorage<Session, MaxSessions, uint8_t, 0> _sessions;
    MISC::ObjStorage<IMech, MaxMechs, uint8_t, 0> _mechs;
};

} // namespace smcp
