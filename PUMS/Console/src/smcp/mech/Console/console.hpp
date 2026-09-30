/**
 * @file console.hpp
 * @brief IConsole + Console<N>: Select/Block/SetTarget TX, Telemetry RX.
 *
 * Наследует Node. Session* — в registry; слоты MaxSessions — в
 * Console / GroupConsole. Банк осей, SessionBank и CMechBank — у leaf.
 *
 * Lifecycle: Node::begin(console_id) → Listen → Ready; start(server_id) — HB.
 * end() — уйти с шины. Смена id — снова begin. Status — SMCP_NODE.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "obj_registry.hpp"
#include "smcp/mech/Console/cmech.hpp"
#include "smcp/mech/message.hpp"
#include "smcp/ilink.hpp"
#include "smcp/node.hpp"
#include "smcp/session.hpp"

namespace smcp {

class IConsole;
class ServerSession;

namespace detail {
/** Регистрация оси в inventory пульта (вызывается из CMech ctor). */
void registerMech(IConsole& cons, CMech& mech) noexcept;
} // namespace detail

/** Узел пульта: CMech inventory, master-сессии к сегментам, Select/Block/SetTarget. */
class IConsole : public Node {
public:
    using MechReg = MISC::ObjRegistry<CMech, uint8_t>;
    template <uint8_t Cap>
    using MechStore = MISC::ObjStorage<CMech, Cap, uint8_t, 0>;

    virtual ~IConsole() = default;

    /** Ёмкость банка сервера. Нет сегмента — 0. */
    [[nodiscard]] uint8_t mechCapacity(uint8_t server_id) const noexcept
    {
        MechReg* const reg = const_cast<IConsole*>(this)->storage(server_id);
        return reg != nullptr ? static_cast<uint8_t>(reg->capacity()) : 0u;
    }
    /** Ось банка @a server_id; нет банка или id — nullptr. */
    [[nodiscard]] CMech* mech(uint8_t server_id, uint8_t id) noexcept
    {
        MechReg* const reg = storage(server_id);
        return reg != nullptr ? reg->get(id) : nullptr;
    }
    [[nodiscard]] const CMech* mech(uint8_t server_id, uint8_t id) const noexcept
    {
        return const_cast<IConsole*>(this)->mech(server_id, id);
    }

    /** Наш id на шине (ILink / Node::id()). */
    [[nodiscard]] uint8_t consoleId() const noexcept { return id(); }

    /**
     * Node::begin, только `isConsoleId`. Не start к серверу.
     */
    void begin(uint8_t console_id) noexcept;
    /** Node::end: closeAllSessions(None) → Idle. */
    void end() noexcept;
    /**
     * Master-сессия к серверу. Только Node::Ready; `isServerId`.
     * Нет слота — onSessionFull, nullptr. Уже есть — тот же указатель.
     * Слоты пульта — ServerSession.
     */
    [[nodiscard]] ServerSession* start(uint8_t server_id) noexcept;
    /** Сессия с этим peer. Нет — nullptr. */
    [[nodiscard]] ServerSession* server(uint8_t server_id) noexcept;

    /** Индекс сегмента по id сервера. Нет такого сервера — 0xFF. Номера сегментов с 0. */
    [[nodiscard]] virtual uint8_t serverIndex(uint8_t server_id) const noexcept = 0;
    /** Id сервера по индексу сегмента. Нет такого сегмента — 0. */
    [[nodiscard]] virtual uint8_t serverId(uint8_t index) const noexcept = 0;

protected:
    friend void detail::registerMech(IConsole& cons, CMech& mech) noexcept;
    friend class CMech;

    explicit IConsole(ILink& link, ClockFn clock) noexcept;

    /**
     * Банк осей сервера. Нет такого сегмента — nullptr.
     * Сессию не проверяет: регистрация раньше Open.
     * Сколько серверов — решает наследник.
     */
    [[nodiscard]] virtual MechReg* storage(uint8_t server_id) noexcept = 0;

    void onPacket(const Packet& pkt) noexcept override;

    /** Open-сессия с src и storage(src) → CMech::onTelemetry. */
    virtual void onTelemetry(uint8_t src_id, const msg::Telemetry& body) noexcept;
};

/**
 * Сессия пульта к одному серверу: Select/Block/SetTarget/GetTelemetry.
 * Слот реестра Node, не обёртка рядом с Session.
 */
class ServerSession : public Session {
public:
    explicit ServerSession(IConsole& console) noexcept;

    /**
     * Select: Add / Remove / Set. Пустая маска и не Set — false.
     * Наследник гасит свой билет до send.
     */
    virtual bool select(msg::Action action, Selection selection) noexcept;
    /** Заменить выделение целиком (Action::Set). */
    bool setSelection(Selection selection) noexcept;
    /** Снять выделение со всех осей. */
    bool clearSelection() noexcept;

    /** Block: Add / Remove / Set. Пустая маска и не Set — false. */
    bool block(msg::Action action, Selection selection) noexcept;
    /** Заменить блок целиком (Action::Set). */
    bool setBlocked(Selection selection) noexcept;
    /** Снять блок со всех осей. */
    bool clearBlocked() noexcept;

    /** Уставка одной оси (класс A). */
    bool setTarget(uint8_t mech_id, const MotionTarget& target) noexcept;

    /**
     * Запрос снимков Telemetry (класс A → Ack + Telemetry D).
     * Пустая маска — все оси сегмента (решает сервер).
     */
    bool getTelemetry(Selection selection) noexcept;
};

/** Console<N>: registry сессий MaxSessions. Банк осей — storage() у leaf. */
template <uint8_t MaxMechs, uint8_t MaxSessions = 1u>
class Console : public IConsole {
public:
    static constexpr uint8_t kMechCount = MaxMechs;
    static constexpr uint8_t kSessionCount = MaxSessions;

    explicit Console(ILink& link, ClockFn clock) noexcept
        : IConsole(link, clock)
    {}

protected:
    [[nodiscard]] SessionReg& sessions() noexcept override { return _sessions; }

private:
    SessionStore<kSessionCount> _sessions;
};

} // namespace smcp
