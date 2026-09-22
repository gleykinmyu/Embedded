/**
 * @file console_message.hpp
 * @brief Northbound PDU (консоль ↔ сервер сегмента). Не транспорт.
 *
 * Demux: SessionConsole (unicast A) / IConsole::onPacket (Telemetry D).
 */

#pragma once

#include <cstdint>

#include "smcp/GroupConsole/group.hpp"
#include "smcp/mech.hpp"
#include "smcp/transport/message.hpp"

namespace smcp {
namespace msg {

/** Northbound MsgId: 0x10… команды, 0x40… telemetry. */
enum class CMsgId : uint8_t {
    Select       = 0x10,
    Block        = 0x11,
    SetTarget    = 0x20,
    GetTelemetry = 0x21,
    Telemetry    = 0x40,
};

/** Адреса northbound. Broadcast 0xFF — транспорт (`kBroadcastId`). */
inline constexpr uint8_t kConsoleIdMin = 0x01u;
inline constexpr uint8_t kConsoleIdMax = 0x0Fu;
/** Сколько консолей / сессий на сервере (id 0x01…0x0F). */
inline constexpr uint8_t kMaxConsoles = kConsoleIdMax - kConsoleIdMin + 1u;
inline constexpr uint8_t kServerIdMin = 0x10u;
inline constexpr uint8_t kServerIdMax = 0xEFu;

/**
 * Коды Nack northbound. Транспорт несёт raw `uint8_t` (Nack::error).
 * Timeout совпадает с Nack::kTimeout (локальный abort Ack в Session).
 */
enum class ErrorCode : uint8_t {
    Ok           = 0x00, /**< Успех (не в Nack; возврат политики acceptSelect). */
    Busy         = 0x01, /**< Ось уже выделена другой консолью. */
    Limits       = 0x02, /**< Концевики / пределы хода (SetTarget). */
    Crc          = 0x03, /**< Ошибка CRC / целостности. */
    MechNotFound = 0x04, /**< Нет механизма с таким id. */
    Safety       = 0x05, /**< Blocked / запрет безопасности. */
    NotReady     = 0x06, /**< Привод не Ready. */
    SelectLimit  = 0x07, /**< Политика сегмента (лимит / зоны Select). */
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
};

[[nodiscard]] inline const char* cstr(Action action) noexcept
{
    switch (action) {
    case Action::Add: return "Add";
    case Action::Remove: return "Remove";
    case Action::Set: return "Set";
    }
    return "?";
}

[[nodiscard]] constexpr bool isValidAction(Action action) noexcept
{
    return action == Action::Add || action == Action::Remove || action == Action::Set;
}

namespace detail {

[[nodiscard]] inline bool unpackMaskOp(const Message& m, Action& action,
                                       Selection& selection) noexcept
{
    if (m.dlc != 5) {
        return false;
    }
    const auto a = static_cast<Action>(m.data[0]);
    if (!isValidAction(a)) {
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
 *    data[0]  data[1..4]     (pkt_id → CAN ID)
 */
struct Select {
    static constexpr uint8_t kId = static_cast<uint8_t>(CMsgId::Select);
    static constexpr bool kNeedsAck = true;

    Action action = Action::Add;
    Selection selection{};

    [[nodiscard]] bool pack(Message& m) const noexcept
    {
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
    static constexpr bool kNeedsAck = true;

    Action action = Action::Add;
    Selection selection{};

    [[nodiscard]] bool pack(Message& m) const noexcept
    {
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
    static constexpr bool kNeedsAck = true;

    Selection selection{};

    [[nodiscard]] bool pack(Message& m) const noexcept
    {
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
 * SetTarget — DLC=7 (accel на шину не кладётся)
 *   +---------+--------------+------------+
 *   | mech_id | target_mm LE | speed LE   |
 *   |   u8    |    i32       | u16 mm/s   |
 *   +---------+--------------+------------+
 *    data[0]   data[1..4]      data[5..6]
 */
struct SetTarget {
    static constexpr uint8_t kId = static_cast<uint8_t>(CMsgId::SetTarget);
    static constexpr bool kNeedsAck = true;
    uint8_t mech_id = 0;
    MotionTarget target{};

    [[nodiscard]] bool pack(Message& m) const noexcept
    {
        return m.push(mech_id)
            && m.pushU32(static_cast<uint32_t>(target.target_mm))
            && m.pushU16(target.speed_mm_s);
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (m.dlc != 7) {
            return false;
        }
        mech_id = m.data[0];
        target.target_mm = static_cast<int32_t>(m.loadU32(1));
        target.speed_mm_s = m.loadU16(5);
        target.accel_mm_s2 = 0;
        return true;
    }
};

/**
 * Telemetry — DLC=8 (broadcast, pkt_id=0)
 *   +---------+----------+--------+--------+--------------+
 *   | mech_id | reserved | holder | status | position LE  |
 *   |   u8    |    u8    |  u8    |   u8   |     i32      |
 *   +---------+----------+--------+--------+--------------+
 */
struct Telemetry {
    static constexpr uint8_t kId = static_cast<uint8_t>(CMsgId::Telemetry);
    static constexpr bool kNeedsAck = false;

    uint8_t mech_id = 0;
    uint8_t reserved = 0;
    uint8_t holder_id = kHolderNone;
    REG::BitMask<IMech::Status> status{};
    int32_t position_mm = 0;

    [[nodiscard]] bool pack(Message& m) const noexcept
    {
        return m.push(mech_id)
            && m.push(reserved)
            && m.push(holder_id)
            && m.push(static_cast<uint8_t>(status.raw()))
            && m.pushU32(static_cast<uint32_t>(position_mm));
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (m.dlc != 8) {
            return false;
        }
        mech_id = m.data[0];
        reserved = m.data[1];
        holder_id = m.data[2];
        status = REG::BitMask<IMech::Status>::from_raw(m.data[3]);
        position_mm = static_cast<int32_t>(m.loadU32(4));
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
    case CMsgId::Telemetry: return "Telemetry";
    }
    return cstrMsg(static_cast<uint8_t>(id));
}

#if defined(SMCP_TRACE_SHORT)
[[nodiscard]] inline const char* cstrPduS(CMsgId id) noexcept
{
    switch (id) {
    case CMsgId::Select: return "Sel";
    case CMsgId::Block: return "Blk";
    case CMsgId::GetTelemetry: return "GT";
    case CMsgId::SetTarget: return "Tg";
    case CMsgId::Telemetry: return "Te";
    }
    return cstrMsgS(static_cast<uint8_t>(id));
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
