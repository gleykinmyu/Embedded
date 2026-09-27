/**
 * @file console.hpp
 * @brief IConsole + Console<N>: Select/Block/SetTarget TX, Telemetry RX.
 *
 * Наследует Node. Session* / CMech* — в registry; primary Session и слоты
 * MaxSessions — в Console / GroupConsole; CMechBank — у leaf.
 *
 * Lifecycle: begin(console_id) → Listen → Ready; start(server_id) — HB.
 * end() — уйти с шины. Смена id — снова begin. Phase — SMCP_CONS.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "ms_timer.hpp"
#include "obj_registry.hpp"
#include "smcp/mech/Console/cmech.hpp"
#include "smcp/mech/message.hpp"
#include "smcp/ilink.hpp"
#include "smcp/node.hpp"
#include "smcp/session.hpp"

namespace smcp {

class IConsole;

namespace detail {
/** Регистрация оси в inventory пульта (вызывается из CMech ctor). */
void registerMech(IConsole& cons, CMech& mech) noexcept;
} // namespace detail

/** Узел пульта: CMech inventory, master-сессии к сегментам, Phase + Select/Block/SetTarget. */
class IConsole : public Node {
public:
    using MechReg = MISC::ObjRegistry<CMech, uint8_t>;
    template <uint8_t Cap>
    using MechStore = MISC::ObjStorage<CMech, Cap, uint8_t, 0>;

    /**
     * Sticky lifecycle узла (не сессии).
     * Fault — IdConflict / RegisterFailed; деталь — Node::getStatus().
     */
    enum class Phase : uint8_t {
        Idle = 0, /**< До begin() / после end(). */
        Listen,   /**< Проверка шины на чужой тот же console_id. */
        Ready,    /**< Listen прошёл; можно start(server_id). */
        Fault,    /**< IdConflict / RegisterFailed. */
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
    /** Ось primary-сегмента по id реестра; нет — nullptr. */
    [[nodiscard]] CMech* mech(uint8_t id) noexcept { return storage().get(id); }
    [[nodiscard]] const CMech* mech(uint8_t id) const noexcept
    {
        return const_cast<IConsole*>(this)->storage().get(id);
    }

    /**
     * Банк осей сегмента @a server_id. Нет / чужой src — nullptr.
     * По умолчанию: `kPrimaryServer` и единственный `start()` → `storage()`.
     * Два start без override — оба src nullptr (leaf обязан перекрыть).
     */
    [[nodiscard]] virtual MechReg* segment(uint8_t server_id) noexcept;

    /** Текущая фаза lifecycle. */
    [[nodiscard]] Phase phase() const noexcept { return _phase; }
    /** Primary-сессия открыта. */
    [[nodiscard]] bool linkUp() const noexcept;
    /** Сессия к @a server_id открыта. `kPrimaryServer` — primary. */
    [[nodiscard]] bool linkUp(uint8_t server_id) const noexcept;

    /** Наш id на шине (ILink / Node::id()). */
    [[nodiscard]] uint8_t consoleId() const noexcept { return id(); }

    /**
     * Свой id на шине + Listen. Не start к серверу.
     * Закрывает все сессии, clearError при Node fault.
     */
    void begin(uint8_t console_id) noexcept;
    /** Уйти с шины: стоп Listen, close всех сессий → Idle. */
    void end() noexcept;
    /**
     * Master-сессия к серверу сегмента. Только Phase::Ready; `isServerId`.
     * Нет слота — onSessionFull, не запоминаем.
     */
    void start(uint8_t server_id) noexcept;
    /** Close сессии к @a server_id, без reconnect. `kPrimaryServer` — primary. */
    void stop(uint8_t server_id) noexcept;

    /** Select: Add / Remove / Set по маске. Пустая маска и не Set — no-op. */
    void select(msg::Action action, Selection selection,
                uint8_t server_id = CMech::kPrimaryServer) noexcept;
    /** Заменить выделение целиком (Action::Set). */
    void setSelection(Selection selection,
                      uint8_t server_id = CMech::kPrimaryServer) noexcept;
    /** Снять выделение со всех осей. */
    void clearSelection(uint8_t server_id = CMech::kPrimaryServer) noexcept;

    /** Block: Add / Remove / Set по маске. Пустая маска и не Set — no-op. */
    void block(msg::Action action, Selection selection,
               uint8_t server_id = CMech::kPrimaryServer) noexcept;
    /** Заменить блок целиком (Action::Set). */
    void setBlocked(Selection selection,
                    uint8_t server_id = CMech::kPrimaryServer) noexcept;
    /** Снять блок со всех осей. */
    void clearBlocked(uint8_t server_id = CMech::kPrimaryServer) noexcept;

    /** Уставка одной оси (класс A). */
    void setTarget(uint8_t mech_id, const MotionTarget& target,
                   uint8_t server_id = CMech::kPrimaryServer) noexcept;

    /**
     * Запрос снимков Telemetry по маске (класс A → Ack + Telemetry D).
     * Пустая маска — все оси сегмента (решает сервер).
     */
    void getTelemetry(Selection selection = {},
                      uint8_t server_id = CMech::kPrimaryServer) noexcept;

protected:
    friend void detail::registerMech(IConsole& cons, CMech& mech) noexcept;
    friend class CMech;

    /** @a primary — слот сессии наследника (Console::_session). */
    explicit IConsole(ILink& link, ClockFn clock, Session& primary) noexcept;

    /** Реестр осей; реализация — Console / leaf. */
    [[nodiscard]] virtual MechReg& storage() noexcept = 0;

    void onPacket(const Packet& pkt) noexcept override;
    void onStatus(Status status) noexcept override;
    /** HB lost: onLink(false); reconnect в update() по списку start(). */
    void onHbLost(Session* session) noexcept override;

    /** По умолчанию: `segment(src)` → CMech::onTelemetry. */
    virtual void onTelemetry(uint8_t src_id, const msg::Telemetry& body) noexcept;
    /** Edge Phase — UI. */
    virtual void onPhase(Phase phase) noexcept { (void)phase; }
    /** Edge линка к серверу (после Open / перед close). */
    virtual void onLink(uint8_t server_id, bool up) noexcept
    {
        (void)server_id;
        (void)up;
    }

    /** Primary-сессия; первый start() занимает её, если Idle. */
    Session& _primary;

private:
    static constexpr uint8_t kMaxStart = 8u;

    void setPhase(Phase phase) noexcept;
    void enterFault() noexcept;
    void closeAllSessions() noexcept;
    void pumpReconnect() noexcept;
    void notifyLinkUp() noexcept;
    [[nodiscard]] Session* sessionTo(uint8_t server_id) noexcept;
    [[nodiscard]] const Session* sessionTo(uint8_t server_id) const noexcept;
    [[nodiscard]] Session* idleSession() noexcept;
    [[nodiscard]] uint8_t startSlot(uint8_t server_id) const noexcept;
    bool rememberStart(uint8_t server_id) noexcept;
    void forgetStart(uint8_t server_id) noexcept;

    Phase _phase = Phase::Idle;
    uint8_t _started[kMaxStart]{};
    bool _link_up[kMaxStart]{};
    MISC::MsTimer _listen{};
};

/** Console<N>: registry сессий MaxSessions + primary Session; CMech — leaf. */
template <uint8_t MaxMechs, uint8_t MaxSessions = 1u>
class Console : public IConsole {
public:
    static constexpr uint8_t kMechCount = MaxMechs;
    static constexpr uint8_t kSessionCount = MaxSessions;

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
