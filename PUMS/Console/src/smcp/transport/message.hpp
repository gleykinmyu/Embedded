/**
 * @file message.hpp
 * @brief Транспорт SMCP по CAN: extended ID + data[0..7], MSG_ID и payload.
 *
 * Правила обмена MVP: PROTOCOL.md
 *
 * CAN ID (29 bit): prio[28:24] | dst[23:16] | src[15:8] | msg_id[7:0]
 * data[]: для request/reply — [pkt_id | payload…]; pkt_id в Packet (сессия), не в body.
 * Heartbeat / Telemetry — только payload (без pkt_id).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>

#include "ican.hpp"
#include "smcp/Console/group.hpp"
#include "smcp/mech.hpp"

namespace smcp {
namespace msg {

inline constexpr uint8_t kConsoleIdMin = 0x01u;
inline constexpr uint8_t kConsoleIdMax = 0x0Fu;
/** Сколько консолей / сессий на сервере (id 0x01…0x0F). */
inline constexpr std::size_t kMaxConsoles = kConsoleIdMax - kConsoleIdMin + 1u;
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
inline constexpr uint8_t kAckRetryControl = 3u;
inline constexpr uint16_t kHeartbeatTimeoutMs = 500u;
/** Сколько интервалов T без RX HB до down (1 = сразу при первом timeout). */
inline constexpr uint8_t kHeartbeatMissMax = 3u;

/** MVP wire set: Ack/Nack, Heartbeat, Select, SetTarget, Telemetry. */
enum class MsgId : uint8_t {
    Ack       = 0x01,
    Nack      = 0x02,
    Heartbeat = 0x03,

    Select    = 0x10,
    SetTarget = 0x20,

    Telemetry = 0x40,
};

enum class ErrorCode : uint8_t {
    Ok           = 0x00, /**< Успех (не в Nack; возврат политики acceptSelect). */
    Busy         = 0x01, /**< Ось уже выделена другой консолью. */
    Limits       = 0x02, /**< Концевики / пределы хода (SetTarget). */
    Crc          = 0x03, /**< Ошибка CRC / целостности. */
    MechNotFound = 0x04, /**< Нет механизма с таким id. */
    Safety       = 0x05, /**< Blocked / запрет безопасности. */
    NotReady     = 0x06, /**< Привод не Ready. */
    SelectLimit  = 0x07, /**< Политика сегмента (лимит / зоны Select). */
    Timeout      = 0x08, /**< Локально: исчерпан retry Ack (не с шины). */
};

/**
 * Логический заголовок SMCP = только CAN ID (без data).
 * prio — 5 бит (0 = высший приоритет арбитража).
 *
 *   CAN ID (29 bit, IDE=1):
 *   +-------+--------+--------+--------+
 *   | prio  |  dst   |  src   | msg_id |
 *   | 5 bit | 8 bit  | 8 bit  | 8 bit  |
 *   +-------+--------+--------+--------+
 *   [28:24]  [23:16]  [15:8]   [7:0]
 */
struct Header {
    uint8_t prio = 15;
    uint8_t src_id = 0;
    uint8_t dst_id = 0;
    MsgId msg_id = MsgId::Heartbeat;

    /** Кадр адресован этому узлу (или broadcast). */
    [[nodiscard]] constexpr bool isAddressedTo(uint8_t node_id) const noexcept
    {
        return dst_id == node_id || dst_id == kBroadcastId;
    }

    /** Только CAN ID; data пишет body. */
    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    /** Разбор ID. */
    [[nodiscard]] bool deserialize(const BIF::CAN::Frame& frame) noexcept;
};

/**
 * Ack — DLC=1: data[0]=pkt_id (поле Packet, не body).
 */
struct Ack {
    static constexpr MsgId kId = MsgId::Ack;

    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] static bool deserialize(const BIF::CAN::Frame& frame, Ack& out) noexcept;
};

/**
 * Nack — DLC=2
 *   +--------+------+
 *   | pkt_id | code |
 *   +--------+------+
 *    data[0]  data[1]   (pkt_id → Packet)
 */
struct Nack {
    static constexpr MsgId kId = MsgId::Nack;
    ErrorCode code = ErrorCode::Busy;

    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] static bool deserialize(const BIF::CAN::Frame& frame, Nack& out) noexcept;
};

/**
 * Heartbeat — DLC=0 (только ID).
 */
struct Heartbeat {
    static constexpr MsgId kId = MsgId::Heartbeat;

    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] static bool deserialize(const BIF::CAN::Frame& frame, Heartbeat& out) noexcept;
};

/**
 * Select — DLC=6
 *   +--------+--------+-------------+
 *   | pkt_id | action | mask LE u32 |
 *   +--------+--------+-------------+
 *    data[0]  data[1]  data[2..5]   (pkt_id → Packet)
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
 * SetTarget — DLC=8 (accel на шину не кладётся)
 *   +--------+---------+--------------+------------+
 *   | pkt_id | mech_id | target_mm LE | speed LE   |
 *   |  u8    |   u8    |    i32       | u16 mm/s   |
 *   +--------+---------+--------------+------------+
 *    data[0]  data[1]   data[2..5]     data[6..7]  (pkt_id → Packet)
 */
struct SetTarget {
    static constexpr MsgId kId = MsgId::SetTarget;
    uint8_t mech_id = 0;
    MotionTarget target;

    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] static bool deserialize(const BIF::CAN::Frame& frame, SetTarget& out) noexcept;
};

/**
 * Telemetry — DLC=7 (broadcast, без pkt_id)
 *   +---------+--------+--------------+--------+
 *   | mech_id | holder | position LE  | status |
 *   |   u8    |  u8    |    i32       |   u8   |
 *   +---------+--------+--------------+--------+
 *    data[0]   data[1]  data[2..5]     data[6]
 */
struct Telemetry {
    static constexpr MsgId kId = MsgId::Telemetry;

    uint8_t mech_id = 0;
    uint8_t holder_id = kHolderNone;
    int32_t position_mm = 0;
    REG::BitMask<IMech::Status> status{};

    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] static bool deserialize(const BIF::CAN::Frame& frame, Telemetry& out) noexcept;
};

using Message = std::variant<Ack,
                             Nack,
                             Heartbeat,
                             Select,
                             SetTarget,
                             Telemetry>;

/**
 * Кадр SMCP: Header (CAN ID) + опциональный session pkt_id + body.
 * pkt_id значим только если helpers::carriesPktId(msg_id); иначе 0.
 */
struct Packet {
    Header hdr{};
    uint8_t pkt_id = 0;
    Message body{};
};

/** Свободные хелперы wire/правил (не поля body). */
namespace helpers {

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

/** Request: ждёт Ack/Nack. */
[[nodiscard]] constexpr bool requiresAck(MsgId id) noexcept
{
    switch (id) {
    case MsgId::Select:
    case MsgId::SetTarget:
        return true;
    case MsgId::Ack:
    case MsgId::Nack:
    case MsgId::Heartbeat:
    case MsgId::Telemetry:
    default:
        return false;
    }
}

/** На wire data[0]=pkt_id (request/reply); body без этого поля. */
[[nodiscard]] constexpr bool carriesPktId(MsgId id) noexcept
{
    switch (id) {
    case MsgId::Ack:
    case MsgId::Nack:
    case MsgId::Select:
    case MsgId::SetTarget:
        return true;
    case MsgId::Heartbeat:
    case MsgId::Telemetry:
    default:
        return false;
    }
}

[[nodiscard]] constexpr uint16_t ackTimeoutMs(MsgId id) noexcept
{
    return requiresAck(id) ? kAckTimeoutControlMs : 0u;
}

[[nodiscard]] constexpr uint8_t ackRetryCount(MsgId id) noexcept
{
    return requiresAck(id) ? kAckRetryControl : 0u;
}

[[nodiscard]] constexpr uint8_t defaultPrio(MsgId id) noexcept
{
    switch (id) {
    case MsgId::Select:
    case MsgId::SetTarget:
        return 2u;
    case MsgId::Ack:
    case MsgId::Nack:
        return 4u;
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

[[nodiscard]] constexpr Header unpackCanId(uint32_t can_id) noexcept
{
    Header hdr{};
    hdr.prio = static_cast<uint8_t>((can_id >> kCanIdPrioPos) & kPrioMax);
    hdr.dst_id = static_cast<uint8_t>((can_id >> kCanIdDstPos) & 0xFFu);
    hdr.src_id = static_cast<uint8_t>((can_id >> kCanIdSrcPos) & 0xFFu);
    hdr.msg_id = static_cast<MsgId>(can_id & 0xFFu);
    return hdr;
}

[[nodiscard]] Header headerFromCanFrame(const BIF::CAN::Frame& frame) noexcept;

[[nodiscard]] bool toCanFrame(Packet packet, BIF::CAN::Frame& out) noexcept;
[[nodiscard]] bool fromCanFrame(const BIF::CAN::Frame& frame, Packet& packet) noexcept;

} // namespace helpers

} // namespace msg
} // namespace smcp
