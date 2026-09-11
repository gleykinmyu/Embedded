/**
 * @file server.hpp
 * @brief IServer + Server<N> + SessionConsole: Select/Block/SetTarget → Ack + Telemetry.
 *
 * Наследует Node. Session* — в registry; объекты Session владеет leaf (MServer).
 * Сервер: до kMaxConsoles сессий (SessionConsole).
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
/** Регистрация оси в inventory сервера (вызывается из DriveMech ctor). */
void registerMech(IServer& server, IMech& mech) noexcept;
} // namespace detail

/** Узел сегмента: inventory IMech* + политика Select/Block/SetTarget + broadcast Telemetry. */
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

    /** Broadcast Telemetry (класс D) по текущему состоянию оси. */
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
     * Leaf при общем лимите сливает с чужими holders (напр. SelectLimit).
     */
    [[nodiscard]] virtual msg::ErrorCode acceptSelect(uint8_t console_id,
                                                      Selection selected) const noexcept
    {
        (void)console_id;
        (void)selected;
        return msg::ErrorCode::Ok;
    }

    /**
     * Можно ли консоли @a console_id выставить сегментную маску @a blocked.
     * @a blocked — итоговое Status::Blocked сегмента после запроса (без commit).
     */
    [[nodiscard]] virtual msg::ErrorCode acceptBlock(uint8_t console_id,
                                                     Selection blocked) const noexcept
    {
        (void)console_id;
        (void)blocked;
        return msg::ErrorCode::Ok;
    }

    /**
     * Можно ли консоли @a console_id задать @a target оси @a mech_id.
     * Leaf: лимиты хода / зоны → ErrorCode::Limits.
     */
    [[nodiscard]] virtual msg::ErrorCode acceptSetTarget(uint8_t console_id,
                                                         uint8_t mech_id,
                                                         const MotionTarget& target) const noexcept
    {
        (void)console_id;
        (void)mech_id;
        (void)target;
        return msg::ErrorCode::Ok;
    }
};

/**
 * Сессия консоли на сервере: class A (Select/Block/SetTarget) → Ack/Nack + Telemetry.
 */
class SessionConsole : public Session {
public:
    explicit SessionConsole(IServer& server) noexcept;

protected:
    // --- RX ---

    [[nodiscard]] bool onPacket(const msg::Packet& pkt) noexcept override;

private:
    /** Select (holder) vs Block (Status::Blocked) — общий пайплайн plan→accept→commit. */
    enum class MaskKind : uint8_t { Select, Block };

    /**
     * План изменения маски для Action Add/Remove/Set (без commit).
     * was = наше владение (Select) или isBlocked (Block).
     */
    struct MaskPlan {
        Selection take{}; /**< Станет true. */
        Selection drop{}; /**< Станет false. */
        Selection next{}; /**< Итог для accept*. */

        void note(uint8_t mid, bool was, bool in_mask, msg::Action action) noexcept;
        [[nodiscard]] Selection changed() const noexcept { return take | drop; }
    };

    /**
     * Общий путь Select/Block: план → accept* → commit → Ack → Telemetry.
     * Пропуск осей с !in_mask && !was (нет изменений).
     */
    void handleMaskOp(MaskKind kind,
                      msg::Action action,
                      Selection selection,
                      uint8_t pkt_id) noexcept;

    /** SetTarget: проверки → acceptSetTarget → setTarget → Ack → Telemetry. */
    void onSetTarget(const msg::SetTarget& body, uint8_t pkt_id) noexcept;

    /**
     * Проверка доступа к оси.
     * @param must_own true = SetTarget (нужна наша + drive);
     *                 false = Select (чужой → Busy; свободная → drive; своя → Ok).
     */
    [[nodiscard]] static msg::ErrorCode mechGuard(const IMech& m,
                                                  uint8_t src,
                                                  bool must_own) noexcept;

    void commitSelect(uint8_t src, const MaskPlan& plan) noexcept;
    void commitBlock(const MaskPlan& plan) noexcept;
    void pushTelemetryMask(Selection mask) noexcept;

    IServer& _server;
};

/** Server<N>: storage сессий и осей; leaf регистрирует SessionConsole / DriveMech. */
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
