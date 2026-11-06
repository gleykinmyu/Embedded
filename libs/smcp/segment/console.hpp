/**
 * @file console.hpp
 * @brief segment::Console + console::{Axis, Session}.
 *
 * Console — фасад пульта (в segment). Axis / Session — в console.
 * SessionSlots / AxisSlots — у leaf (SessionBank / console::AxisStore).
 *
 * Lifecycle: Node::begin(console_id) → Listen → Ready; start(server_id) — HB.
 * Кэш Profile / profileAlloc — show::Console + show::Session (не здесь).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "mem/polyslot.hpp"
#include "segment/axis.hpp"
#include "segment/message.hpp"
#include "smcp/ilink.hpp"
#include "smcp/node.hpp"
#include "smcp/session.hpp"

namespace smcp {
namespace segment {

namespace console {
class Axis;
class Session;
/** Индекс / ёмкость слота оси (SizeT у IPolySlot / PolyStore). */
using idx_t = uint16_t;
/** Inventory осей пульта: IPolySlot&lt;Axis&gt; (leaf — AxisStore). */
using AxisSlots = mem::IPolySlot<Axis, idx_t>;
/** Плотный пул осей: PolyStore&lt;Axis, A&gt;. */
template <idx_t N, typename A = Axis>
using AxisStore = mem::PolyStore<Axis, A, idx_t, N>;
} // namespace console

/**
 * Узел пульта: inventory осей, master-сессии к сегментам, Select/Block/SetTarget.
 * SessionBank + console::AxisStore — у leaf (MConsole / TestConsole).
 */
class Console : public Node {
public:
    virtual ~Console() = default;

    [[nodiscard]] console::idx_t axisCapacity() const noexcept
    {
        return const_cast<Console*>(this)->axes().capacity();
    }
    /** Ось по индексу слота 0…capacity-1. Нет — nullptr. */
    [[nodiscard]] console::Axis* axis(console::idx_t slot) noexcept
    {
        return axes().at(slot);
    }
    [[nodiscard]] const console::Axis* axis(console::idx_t slot) const noexcept
    {
        return const_cast<Console*>(this)->axis(slot);
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
     * Слоты пульта — console::Session (show — show::Session).
     */
    [[nodiscard]] console::Session* start(uint8_t server_id) noexcept;
    /** Сессия с этим peer (server_id). Нет — nullptr. */
    [[nodiscard]] console::Session* session(uint8_t server_id) noexcept;

protected:
    friend class console::Axis;
    friend class console::Session;

    explicit Console(ILink& link, ClockFn clock) noexcept;

    /** Плотный inventory осей пульта. */
    [[nodiscard]] virtual console::AxisSlots& axes() noexcept = 0;
};

namespace console {

/**
 * IAxis пульта: local inventory + live Telemetry.
 * Route — виртуальный (нет копии в базе): 1:1 в наследнике или из Patch (show::Axis).
 * TX / telemetry — по route(); пустой — no-op.
 */
class Axis : public IAxis {
public:
    explicit Axis(Console& console) noexcept;

    [[nodiscard]] Console& console() noexcept { return *_console; }
    [[nodiscard]] const Console& console() const noexcept { return *_console; }

    /**
     * Адрес на шине. База — пусто; наследник: 1:1 или Patch.route.
     * Логический id — у Patch / наследника, не здесь.
     */
    [[nodiscard]] virtual axis::Route route() const noexcept { return {}; }
    [[nodiscard]] bool hasRoute() const noexcept { return !route().empty(); }

    [[nodiscard]] int32_t position() const noexcept override { return _position; }
    [[nodiscard]] REG::BitMask<axis::Status> status() const noexcept override { return _status; }
    [[nodiscard]] uint8_t holder() const noexcept override { return _holder; }

    /** Select/Deselect по route. */
    [[nodiscard]] bool select(uint8_t console_id) noexcept override;
    [[nodiscard]] bool block(bool blocked) noexcept override;
    [[nodiscard]] bool setTarget(const axis::MotionTarget& target) noexcept override;
    [[nodiscard]] bool resetFault() noexcept override;

    /** Зеркало Telemetry (holder / status / position). Show — override (шкала). */
    virtual void onTelemetry(const Telemetry& telemetry) noexcept;

protected:
    Console* _console;
    uint8_t _holder = kHolderNone;
    int32_t _position = 0;
    REG::BitMask<axis::Status> _status{};
};

/**
 * Сессия пульта к одному серверу: Select/Block/SetTarget/GetTelemetry + GET паспортов.
 * Get AxisProfile — в буфер вызывающего (без кэша / аллокатора).
 */
class Session : public ::smcp::Session {
public:
    explicit Session(Console& console) noexcept;

    [[nodiscard]] Console& console() noexcept
    {
        return static_cast<Console&>(node);
    }

    /** AxisRegistered после успешного Get (маска + revision); иначе нули. */
    [[nodiscard]] const AxisRegisteredInfo& axisRegistered() const noexcept
    {
        return _axis_registered;
    }

    /** Get AxisRegistered → onReply: AxisRegisteredInfo in-place (6 байт). */
    [[nodiscard]] session::TaggedOps::Result getAxisRegistered() noexcept;
    /**
     * Get AxisProfile(axis_id) → тело в @a out (wire write-in-place).
     */
    [[nodiscard]] session::TaggedOps::Result getAxisProfile(uint8_t axis_id,
                                             axis::Profile& out) noexcept;

    /** Select: Add / Remove / Set. Пустая маска и не Set — false. */
    bool select(Action action, Selection selection) noexcept;
    /** Заменить выделение целиком (Action::Set). */
    bool setSelection(Selection selection) noexcept;
    /** Снять выделение со всех осей. */
    bool clearSelection() noexcept;

    /** Block: Add / Remove / Set. Пустая маска и не Set — false. */
    bool block(Action action, Selection selection) noexcept;
    /** Заменить блок целиком (Action::Set). */
    bool setBlocked(Selection selection) noexcept;
    /** Снять блок со всех осей. */
    bool clearBlocked() noexcept;

    /** Уставка одной оси (класс A, DLC=8). */
    bool setTarget(uint8_t axis_id, const axis::MotionTarget& target) noexcept;
    /** Та же уставка на всё текущее Select этой консоли (класс A, DLC=7). */
    bool setTarget(const axis::MotionTarget& target) noexcept;

    /**
     * Запрос снимков Telemetry (класс A → Ack + Telemetry D).
     * Пустая маска — все оси сегмента (решает сервер).
     */
    bool getTelemetry(Selection selection) noexcept;

    /**
     * Пустая маска — globalFault сегмента (GPIO сервера).
     * Ненустая — Fault указанных осей.
     */
    bool resetFault(Selection selection) noexcept;

    /**
     * Telemetry с peer этой сессии (Node → sessionByPeer(src)).
     * @return true — обработали; false — игнор (Node → Console по умолчанию).
     */
    [[nodiscard]] virtual bool onTelemetry(const Telemetry& body) noexcept;

protected:
    [[nodiscard]] bool onPacket(const Packet& pkt) noexcept override;
    void onReply(uint8_t tag, uint8_t index, const IBuf& buf,
                 session::MultiByte::Error status) noexcept override;

    /** AxisRegistered; Get пишет сюда in-place (wire = 6 байт). */
    AxisRegisteredInfo _axis_registered{};
};

} // namespace console
} // namespace segment
} // namespace smcp
