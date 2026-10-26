/**
 * @file message.hpp
 * @brief Northbound PDU (консоль ↔ сервер сегмента). Не транспорт.
 *
 * Demux: SessionConsole (unicast A) / IConsole::onPacket (Telemetry D).
 * Get/Set/DataBegin/Data — транспорт `smcp/message.hpp` (Node / Session).
 */

#pragma once

#include <cstdint>

#include "enum_value.hpp"
#include "smcp/show/group.hpp"
#include "smcp/mech/axis.hpp"
#include "smcp/message.hpp"

namespace smcp {
namespace msg {

/**
 * Northbound MsgId (оси / сервис PIN).
 * Get/Set/DataBegin/Data — `msg::Id` в libs/smcp. 0x30 — Heartbeat.
 */
enum class CMsgId : uint8_t {
    /** Пульт берёт или отдаёт оси: маска + Add / Remove / Set. */
    Select = 0x10,
    /** Сегментный запрет движения: маска + Add / Remove / Set. Не флаг группы на пульте. */
    Block = 0x11,
    /** Уставка: одна ось или всё Select консоли (флаг all_selected → DLC 7/8). */
    SetTarget = 0x20,
    /** Снимок осей по маске. Состояние сегмента не меняет. Пустая маска — весь inventory. */
    GetTelemetry = 0x21,
    /** Пустая маска — globalFault (GPIO сервера). Биты — Fault указанных осей. */
    ResetFault = 0x22,
    /** Хэш PIN на эту сессию. Совпал с NVM сервера — Ack, нет — Nack. */
    ServiceUnlock = 0x23,
    /** Снимок оси: кто держит, статус, позиция в единицах kind. */
    Telemetry = 0x40,
};

/** Адреса northbound. Broadcast 0xFF — транспорт (`Packet::kBroadcastId`). */
inline constexpr uint8_t kConsoleIdMin = 0x01u;
inline constexpr uint8_t kConsoleIdMax = 0x0Fu;
/** Сколько консолей / сессий на сервере (id 0x01…0x0F). */
inline constexpr uint8_t kMaxConsoles = kConsoleIdMax - kConsoleIdMin + 1u;
inline constexpr uint8_t kServerIdMin = 0x10u;
inline constexpr uint8_t kServerIdMax = 0xEFu;

/**
 * Коды Nack northbound. Транспорт несёт raw `uint8_t` (Nack::error).
 * Прикладные коды — строго выше @c Nack::Error::Max (0x0F).
 * @c Timeout = @c Nack::kTimeout — локальный abort окна Ack (не с шины).
 */
enum class ErrorCode : uint8_t {
    Ok           = 0x00, /**< Успех (не в Nack; возврат политики acceptSelect). */
    Busy         = 0x10, /**< Ось уже выделена другой консолью. */
    Limits       = 0x11, /**< Концевики / пределы хода (SetTarget). */
    Crc          = 0x12, /**< Ошибка CRC / целостности. */
    MechNotFound = 0x13, /**< Нет механизма с таким id. */
    Safety       = 0x14, /**< Blocked / запрет безопасности. */
    NotReady     = 0x15, /**< Привод не Ready. */
    SelectLimit  = 0x16, /**< Политика сегмента (лимит / зоны Select). */
    Timeout      = Nack::kTimeout, /**< Локально: исчерпан retry Ack (не с шины). */
};

[[nodiscard]] inline const char* cstr(ErrorCode code) noexcept
{
    switch (code) {
    case ErrorCode::Ok: return "Ok";
    case ErrorCode::Busy: return "Busy";
    case ErrorCode::Limits: return "Limits";
    case ErrorCode::Crc: return "Crc";
    case ErrorCode::MechNotFound: return "MechNotFound";
    case ErrorCode::Safety: return "Safety";
    case ErrorCode::NotReady: return "NotReady";
    case ErrorCode::SelectLimit: return "SelectLimit";
    case ErrorCode::Timeout: return "Timeout";
    }
    return "?";
}

#if defined(SMCP_TRACE_SHORT)
[[nodiscard]] inline const char* cstrS(ErrorCode code) noexcept
{
    switch (code) {
    case ErrorCode::Ok: return "Ok";
    case ErrorCode::Busy: return "Busy";
    case ErrorCode::Limits: return "Lim";
    case ErrorCode::Crc: return "Crc";
    case ErrorCode::MechNotFound: return "Mech";
    case ErrorCode::Safety: return "Safe";
    case ErrorCode::NotReady: return "NRdy";
    case ErrorCode::SelectLimit: return "SLim";
    case ErrorCode::Timeout: return "Tmo";
    }
    return "?";
}
#endif

/**
 * Общий action для Select / Block (маска осей).
 * Add/Remove — дельта по битам; Set — абсолютная маска.
 */
enum class Action : uint8_t {
    Add = 0,    /**< Select / Block. */
    Remove = 1, /**< Deselect / Unblock. */
    Set = 2,    /**< Absolute mask. */
    Count,
};

[[nodiscard]] inline const char* cstr(Action action) noexcept
{
    switch (action) {
    case Action::Add: return "Add";
    case Action::Remove: return "Remove";
    case Action::Set: return "Set";
    case Action::Count: break;
    }
    return "?";
}

namespace detail {

[[nodiscard]] inline bool unpackMaskOp(const Message& m, Action& action,
                                       Selection& selection) noexcept
{
    if (m.dlc != 5) {
        return false;
    }
    const auto a = MISC::EnumValue<Action>::from_raw(m.data[0]);
    if (!a.isValid()) {
        return false;
    }
    action = a;
    selection = Selection::from_raw(m.loadU32(1));
    return true;
}

[[nodiscard]] inline bool packMaskOp(Message& m, Action action, Selection selection) noexcept
{
    return m.push(static_cast<uint8_t>(action)) && m.pushU32(selection.raw());
}

} // namespace detail

/**
 * Select — DLC=5
 *   +--------+-------------+
 *   | action | mask LE u32 |
 *   +--------+-------------+
 *    data[0]  data[1..4]     (txn_id → CAN ID)
 */
struct Select {
    static constexpr uint8_t kId = static_cast<uint8_t>(CMsgId::Select);

    Action action = Action::Add;
    Selection selection{};

    [[nodiscard]] bool pack(Message& m) const noexcept
    {
        m = {};
        m.id = kId;
        return detail::packMaskOp(m, action, selection);
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        return detail::unpackMaskOp(m, action, selection);
    }
};

/**
 * Block — DLC=5 (как Select)
 */
struct Block {
    static constexpr uint8_t kId = static_cast<uint8_t>(CMsgId::Block);

    Action action = Action::Add;
    Selection selection{};

    [[nodiscard]] bool pack(Message& m) const noexcept
    {
        m = {};
        m.id = kId;
        return detail::packMaskOp(m, action, selection);
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        return detail::unpackMaskOp(m, action, selection);
    }
};

/**
 * GetTelemetry — DLC=4 (класс A)
 *   +-------------+
 *   | mask LE u32 |
 *   +-------------+
 *    data[0..3]
 *
 * Пустая маска = все оси inventory сервера.
 * Ответ: Ack, затем Telemetry (D).
 */
struct GetTelemetry {
    static constexpr uint8_t kId = static_cast<uint8_t>(CMsgId::GetTelemetry);

    Selection selection{};

    [[nodiscard]] bool pack(Message& m) const noexcept
    {
        m = {};
        m.id = kId;
        return m.pushU32(selection.raw());
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (m.dlc != 4) {
            return false;
        }
        selection = Selection::from_raw(m.loadU32(0));
        return true;
    }
};

/**
 * SetTarget — одна структура; флаг all_selected задаёт DLC.
 *
 * Общий префикс:
 *   +------+-----------+----------+
 *   | mode | target LE | speed LE |
 *   |  u8  |    i32    |   u16    |
 *   +------+-----------+----------+
 *    [0]    [1..4]       [5..6]
 *
 * DLC=7  — all_selected: уставка на все оси, которые эта консоль держит Select.
 * DLC=8  — одна ось: + axis_id в data[7].
 */
struct SetTarget {
    static constexpr uint8_t kId = static_cast<uint8_t>(CMsgId::SetTarget);

    MotionTarget target{};
    uint8_t axis_id = 0;
    bool all_selected = false; /**< true → без axis_id; сервер берёт Select этой консоли. */

    [[nodiscard]] bool pack(Message& m) const noexcept
    {
        m = {};
        m.id = kId;
        if (!m.push(static_cast<uint8_t>(target.mode))
            || !m.pushU32(static_cast<uint32_t>(target.target))
            || !m.pushU16(target.speed)) {
            return false;
        }
        if (all_selected) {
            return true;
        }
        return m.push(axis_id);
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (m.dlc != 7 && m.dlc != 8) {
            return false;
        }
        const auto mode = MISC::EnumValue<MoveMode>::from_raw(m.data[0]);
        if (!mode.isValid()) {
            return false;
        }
        target.mode = mode;
        target.target = static_cast<int32_t>(m.loadU32(1));
        target.speed = m.loadU16(5);
        target._reserved = 0;
        all_selected = (m.dlc == 7);
        axis_id = all_selected ? 0 : m.data[7];
        return true;
    }
};

/**
 * ResetFault — DLC=4 (класс A).
 *   +-------------+
 *   | mask LE u32 |
 *   +-------------+
 *    data[0..3]
 *
 * Пустая маска — globalFault, отдельные GPIO сервера (E-Stop release).
 * Ненулевая — сброс Fault только у указанных осей.
 */
struct ResetFault {
    static constexpr uint8_t kId = static_cast<uint8_t>(CMsgId::ResetFault);

    Selection selection{};

    [[nodiscard]] bool pack(Message& m) const noexcept
    {
        m = {};
        m.id = kId;
        return m.pushU32(selection.raw());
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (m.dlc != 4) {
            return false;
        }
        selection = Selection::from_raw(m.loadU32(0));
        return true;
    }
};

/**
 * Telemetry — DLC=8 (broadcast, txn_id=0)
 *   +---------+----------+--------+--------+--------------+
 *   | axis_id | reserved | holder | status | position LE  |
 *   |   u8    |    u8    |  u8    |   u8   | i32 (kind)   |
 *   +---------+----------+--------+--------+--------------+
 */
struct Telemetry {
    static constexpr uint8_t kId = static_cast<uint8_t>(CMsgId::Telemetry);

    uint8_t axis_id = 0;
    uint8_t reserved = 0;
    uint8_t holder_id = kHolderNone;
    REG::BitMask<IAxis::Status> status{};
    int32_t position = 0;

    [[nodiscard]] bool pack(Message& m) const noexcept
    {
        m = {};
        m.id = kId;
        return m.push(axis_id)
            && m.push(reserved)
            && m.push(holder_id)
            && m.push(static_cast<uint8_t>(status.raw()))
            && m.pushU32(static_cast<uint32_t>(position));
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (m.dlc != 8) {
            return false;
        }
        axis_id = m.data[0];
        reserved = m.data[1];
        holder_id = m.data[2];
        status = REG::BitMask<IAxis::Status>::from_raw(m.data[3]);
        position = static_cast<int32_t>(m.loadU32(4));
        return true;
    }
};

/**
 * ServiceUnlock — DLC=4 (класс A). PIN-хэш; проверка на сервере — фаза 7.
 *   +--------------+
 *   | pin_hash LE  |
 *   +--------------+
 *    data[0..3]
 */
struct ServiceUnlock {
    static constexpr uint8_t kId = static_cast<uint8_t>(CMsgId::ServiceUnlock);

    uint32_t pin_hash = 0;

    [[nodiscard]] bool pack(Message& m) const noexcept
    {
        m = {};
        m.id = kId;
        return m.pushU32(pin_hash);
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (m.dlc != 4) {
            return false;
        }
        pin_hash = m.loadU32(0);
        return true;
    }
};

[[nodiscard]] inline const char* cstrPdu(CMsgId id) noexcept
{
    switch (id) {
    case CMsgId::Select: return "Select";
    case CMsgId::Block: return "Block";
    case CMsgId::GetTelemetry: return "GetTelemetry";
    case CMsgId::SetTarget: return "SetTarget";
    case CMsgId::ResetFault: return "ResetFault";
    case CMsgId::ServiceUnlock: return "ServiceUnlock";
    case CMsgId::Telemetry: return "Telemetry";
    }
    return cstr(static_cast<uint8_t>(id));
}

#if defined(SMCP_TRACE_SHORT)
[[nodiscard]] inline const char* cstrPduS(CMsgId id) noexcept
{
    switch (id) {
    case CMsgId::Select: return "Sel";
    case CMsgId::Block: return "Blk";
    case CMsgId::GetTelemetry: return "GT";
    case CMsgId::SetTarget: return "Tg";
    case CMsgId::ResetFault: return "RF";
    case CMsgId::ServiceUnlock: return "SU";
    case CMsgId::Telemetry: return "Te";
    }
    return cstrS(static_cast<uint8_t>(id));
}
#endif

[[nodiscard]] constexpr uint8_t serverIdForGlobalMech(uint16_t global_id) noexcept
{
    return static_cast<uint8_t>(kServerIdMin + (global_id / kMechCount));
}

[[nodiscard]] constexpr uint8_t mechIdForGlobalMech(uint16_t global_id) noexcept
{
    return static_cast<uint8_t>(global_id % kMechCount);
}

[[nodiscard]] constexpr bool isConsoleId(uint8_t id) noexcept
{
    return id >= kConsoleIdMin && id <= kConsoleIdMax;
}

[[nodiscard]] constexpr bool isServerId(uint8_t id) noexcept
{
    return id >= kServerIdMin && id <= kServerIdMax;
}

} // namespace msg
} // namespace smcp
