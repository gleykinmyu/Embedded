/**
 * @file server.hpp
 * @brief IServer + Server<N> + SessionConsole: Select→Ack + Telemetry(on change).
 *
 * Наследует Node. SessionConsole* — в registry; объекты владеет leaf (MServer).
 * Сервер: много сессий (до kMaxConsoles).
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "obj_registry.hpp"
#include "smcp/Console/group.hpp"
#include "smcp/mech.hpp"
#include "smcp/transport/ilink.hpp"
#include "smcp/transport/message.hpp"
#include "smcp/transport/node.hpp"
#include "smcp/transport/session.hpp"

namespace smcp {

class IServer;
class SessionConsole;

namespace detail {
void registerMech(IServer& server, IMech& mech) noexcept;
} // namespace detail

class IServer : public Node {
public:
    virtual ~IServer() = default;

    // --- механизмы (lookup) ---

    [[nodiscard]] std::size_t mechCapacity() const noexcept
    {
        return const_cast<IServer*>(this)->storage().capacity();
    }

    [[nodiscard]] IMech* mech(uint8_t id) noexcept { return storage().get(id); }
    [[nodiscard]] const IMech* mech(uint8_t id) const noexcept
    {
        return const_cast<IServer*>(this)->storage().get(id);
    }

    // --- исходящие PDU ---

    /** Broadcast Telemetry (класс D) по текущему состоянию mech. */
    void pushTelemetry(uint8_t mech_id) noexcept;

protected:
    friend void detail::registerMech(IServer& server, IMech& mech) noexcept;
    friend class SessionConsole;

    // --- ctor / storage ---

    explicit IServer(ILink& link, ClockFn clock) noexcept;

    [[nodiscard]] virtual MISC::ObjRegistry<IMech, uint8_t>& storage() noexcept = 0;

    // --- политика (leaf) ---

    /**
     * Можно ли консоли @a console_id держать маску @a selected после запроса.
     * @a selected — итоговое владение этой консоли (после take/drop, без commit).
     * Leaf при общем лимите сам сливает с Selected сегмента (чужие ∪ selected).
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

/** Сессия консоли на сервере: class A (Select) + reply. */
class SessionConsole : public Session {
public:
    explicit SessionConsole(IServer& server) noexcept;

protected:
    [[nodiscard]] bool onPacket(const msg::Packet& pkt) noexcept override;
    void onSelect(const msg::Select& body, uint8_t pkt_id) noexcept;

private:
    IServer& _server;
};

template <std::size_t MaxMechs, std::size_t MaxSessions = msg::kMaxConsoles>
class Server : public IServer {
public:
    static constexpr std::size_t kMechCount = MaxMechs;
    static constexpr std::size_t kSessionCount = MaxSessions;

    explicit Server(ILink& link, ClockFn clock) noexcept
        : IServer(link, clock)
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
