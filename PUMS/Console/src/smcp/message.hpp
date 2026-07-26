/**
 * @file message.hpp
 * @brief Транспорт SMCP по CAN: extended ID + data[0..7], MSG_ID и payload.
 *
 * CAN ID (29 bit): prio[28:24] | dst[23:16] | src[15:8] | msg_id[7:0]
 * data[0] = pkt_id всегда (ACK и детект пропусков в цепочке);
 * тело сообщения дописывает payload с data[1].
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>

#include "ican.hpp"
#include "group.hpp"
#include "mech.hpp"

namespace smcp {
namespace msg {

inline constexpr uint8_t kConsoleIdMin = 0x01u;
inline constexpr uint8_t kConsoleIdMax = 0x0Fu;
inline constexpr uint8_t kServerIdMin = 0x10u;
inline constexpr uint8_t kServerIdMax = 0xEFu;
inline constexpr uint8_t kBroadcastId = 0xFFu;

inline constexpr uint8_t kPrioBits = 5u;
inline constexpr uint8_t kPrioMax = (1u << kPrioBits) - 1u;

inline constexpr unsigned kCanIdMsgPos = 0;
inline constexpr unsigned kCanIdSrcPos = 8;
inline constexpr unsigned kCanIdDstPos = 16;
inline constexpr unsigned kCanIdPrioPos = 24;

inline constexpr uint16_t kAckTimeoutControlMs = 100u;
inline constexpr uint16_t kAckTimeoutConfigMs = 200u;
inline constexpr uint8_t kAckRetryControl = 3u;
inline constexpr uint8_t kAckRetryConfig = 2u;
inline constexpr uint16_t kHeartbeatTimeoutMs = 500u;

enum class MsgId : uint8_t {
    Ack          = 0x01,
    Nack         = 0x02,
    Heartbeat    = 0x03,
    SysInfo      = 0x04,
    GetLog       = 0x05,

    Select       = 0x10,
    ResetFault   = 0x12,

    SetTarget    = 0x20,

    SetConfig    = 0x36,
    GetConfig    = 0x37,

    Telemetry    = 0x40,

    CriticalErr  = 0xEE,
};

enum class ErrorCode : uint8_t {
    Busy   = 0x01,
    Limits = 0x02,
    Crc    = 0x03,
    Safety = 0x05,
};

/** data[0] всегда pkt_id (монотонный счётчик TX на узле). */
inline constexpr uint8_t kPktIdOffset = 0u;
inline constexpr uint8_t kPayloadOffset = 1u;

/**
 * Логический заголовок SMCP на CAN.
 * prio — 5 бит (0 = высший приоритет арбитража).
 *
 *   CAN ID (29 bit, IDE=1):
 *   +-------+--------+--------+--------+
 *   | prio  |  dst   |  src   | msg_id |
 *   | 5 bit | 8 bit  | 8 bit  | 8 bit  |
 *   +-------+--------+--------+--------+
 *   [28:24]  [23:16]  [15:8]   [7:0]
 *
 *   data[0] = pkt_id (всегда).
 */
struct Header {
    uint8_t prio = 15;
    uint8_t src_id = 0;
    uint8_t dst_id = 0;
    MsgId msg_id = MsgId::Heartbeat;
    uint8_t pkt_id = 0;

    /** Кадр адресован этому узлу (или broadcast). */
    [[nodiscard]] constexpr bool isAddressedTo(uint8_t node_id) const noexcept
    {
        return dst_id == node_id || dst_id == kBroadcastId;
    }

    /** ID + data[0]=pkt_id (dlc >= 1). */
    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    /** Разбор ID и pkt_id. */
    [[nodiscard]] bool deserialize(const BIF::CAN::Frame& frame) noexcept;
};

/**
 * Ack — DLC=1
 *   ID: … | msg=0x01
 *   +--------+
 *   | pkt_id |
 *   +--------+
 *    data[0]
 */
struct Ack {
    static constexpr MsgId kId = MsgId::Ack;

    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] static bool deserialize(const BIF::CAN::Frame& frame, Ack& out) noexcept;
};

/**
 * Nack — DLC=2
 *   ID: … | msg=0x02
 *   +--------+------+
 *   | pkt_id | code |
 *   +--------+------+
 *    data[0]  data[1]
 */
struct Nack {
    static constexpr MsgId kId = MsgId::Nack;
    ErrorCode code = ErrorCode::Busy;

    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] static bool deserialize(const BIF::CAN::Frame& frame, Nack& out) noexcept;
};

/**
 * Heartbeat — DLC=1
 *   ID: … | msg=0x03
 *   +--------+
 *   | pkt_id |
 *   +--------+
 *    data[0]
 */
struct Heartbeat {
    static constexpr MsgId kId = MsgId::Heartbeat;

    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] static bool deserialize(const BIF::CAN::Frame& frame, Heartbeat& out) noexcept;
};

/**
 * SysInfo — DLC=5
 *   ID: … | msg=0x04
 *   +--------+-----------+--------+---------+
 *   | pkt_id | fw_ver LE | master | checker |
 *   |  u8    |   u16     |  u8    |   u8    |
 *   +--------+-----------+--------+---------+
 *    data[0]  data[1..2]  data[3]  data[4]
 */
struct SysInfo {
    static constexpr MsgId kId = MsgId::SysInfo;
    uint16_t fw_version = 0;
    uint8_t master_status = 0;
    uint8_t checker_status = 0;

    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] static bool deserialize(const BIF::CAN::Frame& frame, SysInfo& out) noexcept;
};

static_assert(sizeof(SysInfo) == 4u);

/**
 * GetLog — DLC=7
 *   ID: … | msg=0x05
 *   +--------+------------+----------+
 *   | pkt_id | offset LE  | count LE |
 *   |  u8    |   u32      |   u16    |
 *   +--------+------------+----------+
 *    data[0]  data[1..4]   data[5..6]
 */
struct GetLog {
    static constexpr MsgId kId = MsgId::GetLog;
    uint32_t offset = 0;
    uint16_t count = 0;

    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] static bool deserialize(const BIF::CAN::Frame& frame, GetLog& out) noexcept;
};

static_assert(sizeof(Nack) == 1u);

/**
 * Select — DLC=6
 *   ID: … | msg=0x10
 *   +--------+--------+-------------+
 *   | pkt_id | action | mask LE u32 |
 *   +--------+--------+-------------+
 *    data[0]  data[1]  data[2..5]
 *
 * Action::Select / Deselect — дельта по битам маски.
 * Action::Set — полная маска владения этой консоли на сегменте
 *   (бит=1 → select src; бит=0 и наш → deselect; чужих не трогаем).
 */
struct Select {
    static constexpr MsgId kId = MsgId::Select;

    enum class Action : uint8_t {
        Select = 0,
        Deselect = 1,
        Set = 2, /**< Absolute: mask = желаемый набор осей этой консоли. */
    };

    Action action = Action::Select;
    Selection selection;

    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] static bool deserialize(const BIF::CAN::Frame& frame, Select& out) noexcept;
};

/**
 * ResetFault — DLC=2
 *   ID: … | msg=0x12
 *   +--------+---------+
 *   | pkt_id | mech_id |
 *   +--------+---------+
 *    data[0]  data[1]
 */
struct ResetFault {
    static constexpr MsgId kId = MsgId::ResetFault;
    uint8_t mech_id = 0;

    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] static bool deserialize(const BIF::CAN::Frame& frame, ResetFault& out) noexcept;
};

/**
 * SetTarget — DLC=8 (accel на шину не кладётся)
 *   ID: … | msg=0x20
 *   +--------+---------+--------------+------------+
 *   | pkt_id | mech_id | target_mm LE | speed LE   |
 *   |  u8    |   u8    |    i32       | u16 mm/s   |
 *   +--------+---------+--------------+------------+
 *    data[0]  data[1]   data[2..5]     data[6..7]
 */
struct SetTarget {
    static constexpr MsgId kId = MsgId::SetTarget;
    uint8_t mech_id = 0;
    MotionTarget target;

    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] static bool deserialize(const BIF::CAN::Frame& frame, SetTarget& out) noexcept;
};

/**
 * SetConfig — DLC=2
 *   ID: … | msg=0x36
 *   +--------+---------+
 *   | pkt_id | mech_id |
 *   +--------+---------+
 *    data[0]  data[1]
 */
struct SetConfig {
    static constexpr MsgId kId = MsgId::SetConfig;
    uint8_t mech_id = 0;

    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] static bool deserialize(const BIF::CAN::Frame& frame, SetConfig& out) noexcept;
};

/**
 * GetConfig — DLC=2
 *   ID: … | msg=0x37
 *   +--------+---------+
 *   | pkt_id | mech_id |
 *   +--------+---------+
 *    data[0]  data[1]
 */
struct GetConfig {
    static constexpr MsgId kId = MsgId::GetConfig;
    uint8_t mech_id = 0;

    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] static bool deserialize(const BIF::CAN::Frame& frame, GetConfig& out) noexcept;
};

/**
 * Telemetry — DLC=8
 *   ID: … | msg=0x40
 *   +--------+---------+--------+--------------+--------+
 *   | pkt_id | mech_id | owner  | position LE  | status |
 *   |  u8    |   u8    |  u8    |    i32       |   u8   |
 *   +--------+---------+--------+--------------+--------+
 *    data[0]  data[1]   data[2]  data[3..6]     data[7]
 */
struct Telemetry {
    static constexpr MsgId kId = MsgId::Telemetry;

    uint8_t mech_id = 0;
    uint8_t select_owner_id = kSelectOwnerNone;
    int32_t position_mm = 0;
    REG::BitMask<IMech::Status> status{};

    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] static bool deserialize(const BIF::CAN::Frame& frame, Telemetry& out) noexcept;
};

/**
 * CriticalErr — DLC=3
 *   ID: … | msg=0xEE
 *   +--------+------+--------+
 *   | pkt_id | code | detail |
 *   +--------+------+--------+
 *    data[0]  data[1] data[2]
 */
struct CriticalErr {
    static constexpr MsgId kId = MsgId::CriticalErr;
    ErrorCode code = ErrorCode::Safety;
    uint8_t detail = 0;

    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] static bool deserialize(const BIF::CAN::Frame& frame, CriticalErr& out) noexcept;
};

static_assert(sizeof(CriticalErr) == 2u);

using Message = std::variant<Ack,
                             Nack,
                             Heartbeat,
                             SysInfo,
                             GetLog,
                             Select,
                             ResetFault,
                             SetTarget,
                             SetConfig,
                             GetConfig,
                             Telemetry,
                             CriticalErr>;

struct Packet {
    Header hdr{};
    Message body{};
};

namespace detail {

template <typename T>
[[nodiscard]] constexpr MsgId msgIdOfBody(const T&) noexcept
{
    return T::kId;
}

} // namespace detail

[[nodiscard]] inline MsgId msgIdOf(const Message& message) noexcept
{
    return std::visit([](const auto& body) { return detail::msgIdOfBody(body); }, message);
}

[[nodiscard]] constexpr bool isConsoleId(uint8_t id) noexcept
{
    return id >= kConsoleIdMin && id <= kConsoleIdMax;
}

[[nodiscard]] constexpr bool isServerId(uint8_t id) noexcept
{
    return id >= kServerIdMin && id <= kServerIdMax;
}

[[nodiscard]] constexpr bool isBroadcastId(uint8_t id) noexcept
{
    return id == kBroadcastId;
}

[[nodiscard]] constexpr uint8_t serverIdForGlobalMech(uint16_t global_id) noexcept
{
    return static_cast<uint8_t>(kServerIdMin + (global_id / kMechCount));
}

[[nodiscard]] constexpr uint8_t mechIdForGlobalMech(uint16_t global_id) noexcept
{
    return static_cast<uint8_t>(global_id % kMechCount);
}

[[nodiscard]] constexpr bool requiresAck(MsgId id) noexcept
{
    switch (id) {
    case MsgId::Ack:
    case MsgId::Nack:
    case MsgId::Heartbeat:
    case MsgId::SysInfo:
    case MsgId::Telemetry:
    case MsgId::CriticalErr:
        return false;
    default:
        return true;
    }
}

[[nodiscard]] constexpr uint16_t ackTimeoutMs(MsgId id) noexcept
{
    switch (id) {
    case MsgId::SetConfig:
    case MsgId::GetConfig:
        return kAckTimeoutConfigMs;
    default:
        return requiresAck(id) ? kAckTimeoutControlMs : 0u;
    }
}

[[nodiscard]] constexpr uint8_t ackRetryCount(MsgId id) noexcept
{
    switch (id) {
    case MsgId::SetConfig:
    case MsgId::GetConfig:
        return kAckRetryConfig;
    case MsgId::Select:
    case MsgId::ResetFault:
    case MsgId::SetTarget:
    case MsgId::GetLog:
        return kAckRetryControl;
    default:
        return 0u;
    }
}

[[nodiscard]] constexpr uint8_t defaultPrio(MsgId id) noexcept
{
    switch (id) {
    case MsgId::CriticalErr:
        return 0u;
    case MsgId::Select:
    case MsgId::ResetFault:
    case MsgId::SetTarget:
        return 2u;
    case MsgId::Ack:
    case MsgId::Nack:
        return 4u;
    case MsgId::GetLog:
    case MsgId::SetConfig:
    case MsgId::GetConfig:
        return 10u;
    case MsgId::SysInfo:
        return 16u;
    case MsgId::Telemetry:
        return 20u;
    case MsgId::Heartbeat:
        return 28u;
    default:
        return 15u;
    }
}

[[nodiscard]] constexpr uint32_t packCanId(const Header& hdr) noexcept
{
    const uint32_t prio = static_cast<uint32_t>(hdr.prio & kPrioMax);
    return (prio << kCanIdPrioPos) | (static_cast<uint32_t>(hdr.dst_id) << kCanIdDstPos)
         | (static_cast<uint32_t>(hdr.src_id) << kCanIdSrcPos)
         | static_cast<uint32_t>(hdr.msg_id);
}

[[nodiscard]] constexpr Header unpackCanId(uint32_t can_id, uint8_t pkt_id = 0) noexcept
{
    Header hdr{};
    hdr.prio = static_cast<uint8_t>((can_id >> kCanIdPrioPos) & kPrioMax);
    hdr.dst_id = static_cast<uint8_t>((can_id >> kCanIdDstPos) & 0xFFu);
    hdr.src_id = static_cast<uint8_t>((can_id >> kCanIdSrcPos) & 0xFFu);
    hdr.msg_id = static_cast<MsgId>(can_id & 0xFFu);
    hdr.pkt_id = pkt_id;
    return hdr;
}

[[nodiscard]] Header headerFromCanFrame(const BIF::CAN::Frame& frame) noexcept;

[[nodiscard]] bool toCanFrame(Packet packet, BIF::CAN::Frame& out) noexcept;
[[nodiscard]] bool fromCanFrame(const BIF::CAN::Frame& frame, Packet& packet) noexcept;

} // namespace msg
} // namespace smcp
