/**
 * @file console.hpp
 * @brief IConsole + Console<N>: inventory IMech*, Select/Block TX, Telemetry RX.
 *
 * Наследует Node. Session* — в registry; объекты Session владеет leaf (напр. MConsole).
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "obj_registry.hpp"
#include "smcp/mech.hpp"
#include "smcp/transport/ilink.hpp"
#include "smcp/transport/message.hpp"
#include "smcp/transport/node.hpp"
#include "smcp/transport/session.hpp"

namespace smcp {

class IConsole;

namespace detail {
void registerMech(IConsole& cons, IMech& mech) noexcept;
} // namespace detail

class IConsole : public Node {
public:
    virtual ~IConsole() = default;

    std::size_t mechCapacity() const noexcept
    {
        return const_cast<IConsole*>(this)->storage().capacity();
    }

    IMech* mech(uint8_t id) noexcept { return storage().get(id); }
    const IMech* mech(uint8_t id) const noexcept
    {
        return const_cast<IConsole*>(this)->storage().get(id);
    }

    /** Обычно слот 0 — единственная сессия к серверу. */
    [[nodiscard]] Session* primarySession() noexcept { return session(0); }
    [[nodiscard]] const Session* primarySession() const noexcept { return session(0); }

    [[nodiscard]] uint8_t serverId() const noexcept;

    void startSession(uint8_t server_id) noexcept;
    void stopSession() noexcept;

    [[nodiscard]] bool linkUp() const noexcept;

    void select(msg::Action action, Selection selection) noexcept;
    void setSelection(Selection selection) noexcept;
    void clearSelection() noexcept;

    void block(msg::Action action, Selection selection) noexcept;
    void setBlocked(Selection selection) noexcept;
    void clearBlocked() noexcept;

protected:
    friend void detail::registerMech(IConsole& cons, IMech& mech) noexcept;

    explicit IConsole(ILink& link, ClockFn clock) noexcept;

    [[nodiscard]] virtual MISC::ObjRegistry<IMech, uint8_t>& storage() noexcept = 0;

    void onPacket(const msg::Packet& pkt) noexcept override;
    /** Leaf (MConsole): проброс в Mech::onTelemetry. */
    virtual void handleTelemetry(const msg::Header& hdr, const msg::Telemetry& body) noexcept;
};

template <std::size_t MaxMechs, std::size_t MaxSessions = 1u>
class Console : public IConsole {
public:
    static constexpr std::size_t kMechCount = MaxMechs;
    static constexpr std::size_t kSessionCount = MaxSessions;

    explicit Console(ILink& link, ClockFn clock) noexcept
        : IConsole(link, clock)
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
