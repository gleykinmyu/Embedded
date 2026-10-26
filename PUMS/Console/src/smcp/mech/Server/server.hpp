/**
 * @file server.hpp
 * @brief IServer + Server<N> + SessionConsole: Select/Block/GetTelemetry/SetTarget/ResetFault.
 *
 * Наследует Node. Session* — в registry; объекты Session владеет leaf (MServer).
 * Сервер: до kMaxConsoles сессий (SessionConsole).
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "obj_registry.hpp"
#include "smcp/show/group.hpp"
#include "smcp/mech/axis.hpp"
#include "smcp/mech/message.hpp"
#include "smcp/ilink.hpp"
#include "smcp/node.hpp"
#include "smcp/session.hpp"

namespace smcp {

class IServer;
class SessionConsole;

namespace detail {
/** Регистрация оси в inventory сервера (вызывается из DriveMech ctor). */
void registerMech(IServer& server, IAxis& mech) noexcept;
} // namespace detail

/** Узел сегмента: inventory IAxis* + политика Select/Block/SetTarget + broadcast Telemetry. */
class IServer : public Node {
public:
    using MechReg = MISC::ObjRegistry<IAxis, uint8_t>;
    template <uint8_t Cap>
    using MechStore = MISC::ObjStorage<IAxis, Cap, uint8_t, 0>;

    virtual ~IServer() = default;

    // --- механизмы (lookup) ---

    [[nodiscard]] uint8_t mechCapacity() const noexcept
    {
        return static_cast<uint8_t>(const_cast<IServer*>(this)->storage().capacity());
    }

    [[nodiscard]] IAxis* mech(uint8_t id) noexcept { return storage().get(id); }
    [[nodiscard]] const IAxis* mech(uint8_t id) const noexcept
    {
        return const_cast<IServer*>(this)->storage().get(id);
    }
    /** Оси, прошедшие registerMech. */
    [[nodiscard]] Selection mechRegistered() const noexcept { return _mechRegistered; }

    /**
     * Node::begin, только `isServerId`. Listen → Ready в Node::update.
     */
    void begin(uint8_t server_id) noexcept;

    // --- исходящие PDU ---

    /** Broadcast Telemetry (класс D) по текущему состоянию оси. */
    void pushTelemetry(uint8_t axis_id) noexcept;

protected:
    /** session == nullptr (свой IdConflict): снять оставшиеся Select без Telemetry. */
    void onFault(Session* session, Fault reason) noexcept override;

    friend void detail::registerMech(IServer& server, IAxis& mech) noexcept;
    friend class SessionConsole;

    // --- ctor / storage ---

    explicit IServer(ILink& link, ClockFn clock) noexcept;

    [[nodiscard]] virtual MechReg& storage() noexcept = 0;

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
     * Можно ли консоли @a console_id задать @a target оси @a axis_id.
     * Leaf: лимиты хода / зоны → ErrorCode::Limits.
     */
    [[nodiscard]] virtual msg::ErrorCode acceptSetTarget(uint8_t console_id,
                                                         uint8_t axis_id,
                                                         const MotionTarget& target) const noexcept
    {
        (void)console_id;
        (void)axis_id;
        (void)target;
        return msg::ErrorCode::Ok;
    }

    /**
     * Пустая маска ResetFault: сброс globalFault.
     * Это GPIO сегмента, не оси. Пин — в leaf; база ничего не трогает.
     */
    virtual void resetGlobalFault() noexcept {}

private:
    void dropPeerSelect(Session* session, bool telemetry) noexcept;
    Selection _mechRegistered{};
};

/**
 * Сессия консоли на сервере: class A → Ack/Nack + Telemetry.
 * ServiceUnlock: PIN совпал → Ack, иначе Nack (проверка — фаза 7).
 * Get/Set default Session → Nack NotSupported; тело — Session/MultiByte. Теги — фаза 7.
 */
class SessionConsole : public Session {
public:
    explicit SessionConsole(IServer& server) noexcept;

protected:
    // --- RX ---

    [[nodiscard]] bool onPacket(const Packet& pkt) noexcept override;
    void onFault(Fault reason) noexcept override;

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
                      uint8_t txn_id) noexcept;

    /**
     * SetTarget: одна ось или all_selected (Select этой консоли).
     * Сначала проверка всех → commit → Ack → Telemetry.
     */
    void onSetTarget(const msg::SetTarget& body, uint8_t txn_id) noexcept;

    /** GetTelemetry: проверка маски → Ack → Telemetry по осям (без commit). */
    void onGetTelemetry(const msg::GetTelemetry& body, uint8_t txn_id) noexcept;

    /**
     * Пустая маска → resetGlobalFault, Ack, без Telemetry.
     * Биты → IAxis::resetFault по осям, Ack + Telemetry. Чужая ось — Nack MechNotFound.
     */
    void onResetFault(const msg::ResetFault& body, uint8_t txn_id) noexcept;

    /**
     * Бит маски не в mechRegistered → Nack MechNotFound, detail = лишние биты.
     * @return true — уже Nack, выйти.
     */
    [[nodiscard]] bool checkMechId(Selection selection, uint8_t txn_id) noexcept;

    /**
     * Проверка доступа к оси.
     * @param must_own true = SetTarget (нужна наша + drive);
     *                 false = Select (чужой → Busy; свободная → drive; своя → Ok).
     */
    [[nodiscard]] static msg::ErrorCode mechGuard(const IAxis& m,
                                                  uint8_t src,
                                                  bool must_own) noexcept;

    void commitSelect(uint8_t src, const MaskPlan& plan) noexcept;
    void commitBlock(const MaskPlan& plan) noexcept;
    void pushTelemetryMask(Selection mask) noexcept;

    IServer& _server;
};

/** Server<N>: storage сессий и осей; leaf регистрирует SessionConsole / DriveMech. */
template <uint8_t MaxMechs, uint8_t MaxSessions = msg::kMaxConsoles>
class Server : public IServer {
public:
    static constexpr uint8_t kMechCount = MaxMechs;
    static constexpr uint8_t kSessionCount = MaxSessions;

    explicit Server(ILink& link, ClockFn clock) noexcept
        : IServer(link, clock)
    {}

protected:
    [[nodiscard]] MechReg& storage() noexcept override { return _mechs; }
    [[nodiscard]] SessionReg& sessions() noexcept override { return _sessions; }

    SessionStore<MaxSessions> _sessions;
    MechStore<MaxMechs> _mechs;
};

} // namespace smcp
