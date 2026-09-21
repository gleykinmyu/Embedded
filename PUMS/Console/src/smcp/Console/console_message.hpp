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
inline constexpr uint8_t kMsgSelect = 0x10u;
inline constexpr uint8_t kMsgBlock = 0x11u;
inline constexpr uint8_t kMsgSetTarget = 0x20u;
inline constexpr uint8_t kMsgGetTelemetry = 0x21u;
inline constexpr uint8_t kMsgTelemetry = 0x40u;

/** Адреса northbound. Broadcast 0xFF — транспорт (`kBroadcastId`). */
inline constexpr uint8_t kConsoleIdMin = 0x01u;
inline constexpr uint8_t kConsoleIdMax = 0x0Fu;
/** Сколько консолей / сессий на сервере (id 0x01…0x0F). */
inline constexpr uint8_t kMaxConsoles = kConsoleIdMax - kConsoleIdMin + 1u;
inline constexpr uint8_t kServerIdMin = 0x10u;
inline constexpr uint8_t kServerIdMax = 0xEFu;

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
    if (!m.expectDlc(5)) {
        return false;
    }
    const auto a = static_cast<Action>(m.data[0]);
    if (!isValidAction(a)) {
        return false;
    }
    action = a;
    selection = Selection::from_raw(m.load_le32(1));
    return true;
}

inline void packMaskOp(Message& m, Action action, Selection selection) noexcept
{
    (void)m.push(static_cast<uint8_t>(action));
    (void)m.push_le32(selection.raw());
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
    static constexpr uint8_t kId = kMsgSelect;
    static constexpr bool kNeedsAck = true;

    Action action = Action::Add;
    Selection selection{};

    void pack(Message& m) const noexcept
    {
        detail::packMaskOp(m, action, selection);
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
    static constexpr uint8_t kId = kMsgBlock;
    static constexpr bool kNeedsAck = true;

    Action action = Action::Add;
    Selection selection{};

    void pack(Message& m) const noexcept
    {
        detail::packMaskOp(m, action, selection);
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
    static constexpr uint8_t kId = kMsgGetTelemetry;
    static constexpr bool kNeedsAck = true;

    Selection selection{};

    void pack(Message& m) const noexcept
    {
        (void)m.push_le32(selection.raw());
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (!m.expectDlc(4)) {
            return false;
        }
        selection = Selection::from_raw(m.load_le32(0));
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
    static constexpr uint8_t kId = kMsgSetTarget;
    static constexpr bool kNeedsAck = true;
    uint8_t mech_id = 0;
    MotionTarget target{};

    void pack(Message& m) const noexcept
    {
        (void)m.push(mech_id);
        (void)m.push_le32(static_cast<uint32_t>(target.target_mm));
        (void)m.push_le16(target.speed_mm_s);
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (!m.expectDlc(7)) {
            return false;
        }
        mech_id = m.data[0];
        target.target_mm = static_cast<int32_t>(m.load_le32(1));
        target.speed_mm_s = m.load_le16(5);
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
    static constexpr uint8_t kId = kMsgTelemetry;
    static constexpr bool kNeedsAck = false;

    uint8_t mech_id = 0;
    uint8_t reserved = 0;
    uint8_t holder_id = kHolderNone;
    REG::BitMask<IMech::Status> status{};
    int32_t position_mm = 0;

    void pack(Message& m) const noexcept
    {
        (void)m.push(mech_id);
        (void)m.push(reserved);
        (void)m.push(holder_id);
        (void)m.push(static_cast<uint8_t>(status.raw()));
        (void)m.push_le32(static_cast<uint32_t>(position_mm));
    }
    [[nodiscard]] bool unpack(const Message& m) noexcept
    {
        if (!m.expectDlc(8)) {
            return false;
        }
        mech_id = m.data[0];
        reserved = m.data[1];
        holder_id = m.data[2];
        status = REG::BitMask<IMech::Status>::from_raw(m.data[3]);
        position_mm = static_cast<int32_t>(m.load_le32(4));
        return true;
    }
};

[[nodiscard]] inline const char* cstrPdu(uint8_t id) noexcept
{
    switch (id) {
    case kMsgSelect: return "Select";
    case kMsgBlock: return "Block";
    case kMsgGetTelemetry: return "GetTelemetry";
    case kMsgSetTarget: return "SetTarget";
    case kMsgTelemetry: return "Telemetry";
    default: return cstrMsg(id);
    }
}

#if defined(SMCP_TRACE_SHORT)
[[nodiscard]] inline const char* cstrPduS(uint8_t id) noexcept
{
    switch (id) {
    case kMsgSelect: return "Sel";
    case kMsgBlock: return "Blk";
    case kMsgGetTelemetry: return "GT";
    case kMsgSetTarget: return "Tg";
    case kMsgTelemetry: return "Te";
    default: return cstrMsgS(id);
    }
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
