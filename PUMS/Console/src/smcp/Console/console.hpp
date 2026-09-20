/**
 * @file console.hpp
 * @brief IConsole + Console<N>: Select/Block/SetTarget TX, Telemetry RX.
 *
 * Наследует Node. Session* / CMech* — в registry; primary Session и слоты
 * MaxSessions+1 — в Console / GroupConsole; CMechBank — у leaf.
 *
 * Lifecycle: begin → Listen → Connecting → Online; Fault / onPhase для UI.
 * setConsoleId → снова begin (Listen на конфликт id). Phase — SMCP_CONS.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "ms_timer.hpp"
#include "obj_registry.hpp"
#include "smcp/Console/cmech.hpp"
#include "smcp/transport/ilink.hpp"
#include "smcp/transport/message.hpp"
#include "smcp/transport/node.hpp"
#include "smcp/transport/session.hpp"

namespace smcp {

class IConsole;

namespace detail {
/** Регистрация оси в inventory пульта (вызывается из CMech ctor). */
void registerMech(IConsole& cons, CMech& mech) noexcept;
} // namespace detail

/** Узел пульта: CMech inventory, primary-сессия к серверу, Phase + Select/Block/SetTarget. */
class IConsole : public Node {
public:
    using MechReg = MISC::ObjRegistry<CMech, uint8_t>;
    template <uint8_t Cap>
    using MechStore = MISC::ObjStorage<CMech, Cap, uint8_t, 0>;

    /**
     * Sticky lifecycle (одна primary-сессия).
     * Fault — IdConflict / RegisterFailed; деталь — Node::getStatus().
     */
    enum class Phase : uint8_t {
        Idle = 0,      /**< До begin(). */
        Listen,        /**< Проверка шины на чужой тот же id. */
        Connecting,    /**< HB к серверу (после Listen или после потери линка). */
        Online,        /**< Линк up. */
        Fault,         /**< IdConflict / RegisterFailed. */
    };

    /** Имя фазы для лога / UI. */
    [[nodiscard]] static const char* cstr(Phase phase) noexcept;

    virtual ~IConsole() = default;

    /** Node::update + tick Phase. Вызывать из app loop (не через Node&). */
    void update() noexcept;

    /** Ёмкость банка CMech (MaxMechs у Console<N>). */
    [[nodiscard]] uint8_t mechCapacity() const noexcept
    {
        return static_cast<uint8_t>(const_cast<IConsole*>(this)->storage().capacity());
    }
    /** Ось по id реестра; нет — nullptr. */
    [[nodiscard]] CMech* mech(uint8_t id) noexcept { return storage().get(id); }
    [[nodiscard]] const CMech* mech(uint8_t id) const noexcept
    {
        return const_cast<IConsole*>(this)->storage().get(id);
    }

    /** Текущая фаза lifecycle. */
    [[nodiscard]] Phase phase() const noexcept { return _phase; }
    /** Primary-сессия открыта. */
    [[nodiscard]] bool linkUp() const noexcept;

    /** Наш id на шине (ILink / Node::id()). */
    [[nodiscard]] uint8_t consoleId() const noexcept { return id(); }
    /**
     * Сменить id на шине и снова begin(serverId), если сервер уже задан
     * (новый Listen на IdConflict).
     */
    void setConsoleId(uint8_t console_id) noexcept;

    /** Цель primary-сессии (из последнего begin). */
    [[nodiscard]] uint8_t serverId() const noexcept { return _server_id; }

    /**
     * Старт: Listen → Connecting… (@a server_id ≠ 0).
     * Закрывает primary, clearError при Node fault.
     */
    void begin(uint8_t server_id) noexcept;

    /** Select: Add / Remove / Set по маске. Пустая маска и не Set — no-op. */
    void select(msg::Action action, Selection selection) noexcept;
    /** Заменить выделение целиком (Action::Set). */
    void setSelection(Selection selection) noexcept;
    /** Снять выделение со всех осей. */
    void clearSelection() noexcept;

    /** Block: Add / Remove / Set по маске. Пустая маска и не Set — no-op. */
    void block(msg::Action action, Selection selection) noexcept;
    /** Заменить блок целиком (Action::Set). */
    void setBlocked(Selection selection) noexcept;
    /** Снять блок со всех осей. */
    void clearBlocked() noexcept;

    /** Уставка одной оси (класс A через primary). */
    void setTarget(uint8_t mech_id, const MotionTarget& target) noexcept;

    /**
     * Запрос снимков Telemetry по маске (класс A → Ack + Telemetry D).
     * Пустая маска — все оси сегмента (решает сервер).
     */
    void getTelemetry(Selection selection = {}) noexcept;

protected:
    friend void detail::registerMech(IConsole& cons, CMech& mech) noexcept;
    friend class CMech;

    /** @a primary — слот сессии наследника (Console::_session). */
    explicit IConsole(ILink& link, ClockFn clock, Session& primary) noexcept;

    /** Реестр осей; реализация — Console / leaf. */
    [[nodiscard]] virtual MechReg& storage() noexcept = 0;

    void onPacket(const msg::Packet& pkt) noexcept override;
    void onStatus(Status status) noexcept override;
    /** Потеря HB primary → Connecting. */
    void onHbLost(Session* session) noexcept override;

    /** По умолчанию: CMech::onTelemetry. */
    virtual void onTelemetry(const msg::Header& hdr, const msg::Telemetry& body) noexcept;
    /** Edge Phase — UI. */
    virtual void onPhase(Phase phase) noexcept { (void)phase; }
    /** Согласие наследника на Connecting → Online. По умолчанию сразу. */
    [[nodiscard]] virtual bool readyForOnline() const noexcept { return true; }
    /** Connecting + linkUp + readyForOnline → Online. */
    void tryGoOnline() noexcept;

    /** Primary-сессия к серверу; leaf сравнивает указатель в onHbLost. */
    Session& _primary;

private:
    /** Смена фазы с логом и onPhase. */
    void setPhase(Phase phase) noexcept;
    /** Закрыть primary и уйти в Fault. */
    void enterFault() noexcept;
    /** start(_server_id), если сервер задан. */
    void startPrimary() noexcept;

    Phase _phase = Phase::Idle;
    uint8_t _server_id = 0;
    MISC::MsTimer _listen{};
};

/** Console<N>: registry сессий MaxSessions+1 + primary Session; CMech — leaf. */
template <uint8_t MaxMechs, uint8_t MaxSessions = 1u>
class Console : public IConsole {
public:
    static constexpr uint8_t kMechCount = MaxMechs;
    static constexpr uint8_t kSessionCount = static_cast<uint8_t>(MaxSessions + 1u);

    /** Primary-сессия — _session, слот 0 реестра. */
    explicit Console(ILink& link, ClockFn clock) noexcept
        : IConsole(link, clock, _session)
        , _session(*this)
    {}

protected:
    [[nodiscard]] MechReg& storage() noexcept override { return _mechs; }
    [[nodiscard]] SessionReg& sessions() noexcept override { return _sessions; }

private:
    /* _sessions до _session: Session ctor → register → sessions(). */
    SessionStore<kSessionCount> _sessions;
    Session _session;
    MechStore<MaxMechs> _mechs;
};

} // namespace smcp
