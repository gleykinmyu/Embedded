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
inline constexpr uint8_t kMaxConsoles = kConsoleIdMax - kConsoleIdMin + 1u;
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

/** MVP wire set: Ack/Nack, Heartbeat, Select, Block, GetTelemetry, SetTarget, Telemetry. */
enum class MsgId : uint8_t {
    Ack       = 0x01,
    Nack      = 0x02,
    Heartbeat = 0x03,

    Select       = 0x10,
    Block        = 0x11,

    SetTarget    = 0x20,

    Telemetry = 0x40,
    GetTelemetry = 0x41
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

[[nodiscard]] inline const char* cstr(MsgId id) noexcept
{
    switch (id) {
    case MsgId::Ack: return "Ack";
    case MsgId::Nack: return "Nack";
    case MsgId::Heartbeat: return "Heartbeat";
    case MsgId::Select: return "Select";
    case MsgId::Block: return "Block";
    case MsgId::GetTelemetry: return "GetTelemetry";
    case MsgId::SetTarget: return "SetTarget";
    case MsgId::Telemetry: return "Telemetry";
    }
    return "?";
}

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
 * Nack — DLC=3
 *   +--------+------+--------+
 *   | pkt_id | code | detail |
 *   +--------+------+--------+
 *    data[0]  data[1] data[2]   (pkt_id → Packet)
 *
 * @a detail — контекст отказа (обычно mech_id); @c kNackDetailNone = нет.
 */
inline constexpr uint8_t kNackDetailNone = 0xFFu;

struct Nack {
    static constexpr MsgId kId = MsgId::Nack;
    ErrorCode code = ErrorCode::Busy;
    uint8_t detail = kNackDetailNone;

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

/**
 * Select — DLC=6
 *   +--------+--------+-------------+
 *   | pkt_id | action | mask LE u32 |
 *   +--------+--------+-------------+
 *    data[0]  data[1]  data[2..5]   (pkt_id → Packet)
 *
 * Action::Add / Remove — дельта по битам маски.
 * Action::Set — полная маска владения этой консоли на сегменте
 *   (бит=1 → select src; бит=0 и наш → deselect; чужих не трогаем).
 */
struct Select {
    static constexpr MsgId kId = MsgId::Select;

    Action action = Action::Add;
    Selection selection;

    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] static bool deserialize(const BIF::CAN::Frame& frame, Select& out) noexcept;
};

/**
 * Block — DLC=6 (как Select)
 *   +--------+--------+-------------+
 *   | pkt_id | action | mask LE u32 |
 *   +--------+--------+-------------+
 *    data[0]  data[1]  data[2..5]   (pkt_id → Packet)
 *
 * Action::Add / Remove — дельта по битам маски (сегментный Status::Blocked).
 * Action::Set — полная маска blocked на сегменте
 *   (бит=1 → block; бит=0 → unblock).
 */
struct Block {
    static constexpr MsgId kId = MsgId::Block;

    Action action = Action::Add;
    Selection selection;

    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] static bool deserialize(const BIF::CAN::Frame& frame, Block& out) noexcept;
};

/**
 * GetTelemetry — DLC=5 (класс A)
 *   +--------+-------------+
 *   | pkt_id | mask LE u32 |
 *   +--------+-------------+
 *    data[0]  data[1..4]   (pkt_id → Packet)
 *
 * Запрос снимков Telemetry по битам маски.
 * Пустая маска = все оси inventory сервера (0…cap−1, где mech есть).
 * Ответ: Ack, затем Telemetry (D) по запрошенным осям (без commit состояния).
 */
struct GetTelemetry {
    static constexpr MsgId kId = MsgId::GetTelemetry;

    Selection selection;

    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] static bool deserialize(const BIF::CAN::Frame& frame, GetTelemetry& out) noexcept;
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
 * Telemetry — DLC=8 (broadcast, без pkt_id)
 *   +---------+----------+--------+--------+--------------+
 *   | mech_id | reserved | holder | status | position LE  |
 *   |   u8    |    u8    |  u8    |   u8   |     i32      |
 *   +---------+----------+--------+--------+--------------+
 *    data[0]   data[1]    data[2]  data[3]  data[4..7]
 *
 * reserved после mech_id — слот под dual-axis / расширение адресации.
 * position с data[4]: i32 выровнен, первые 4 байта — заголовок оси.
 */
struct Telemetry {
    static constexpr MsgId kId = MsgId::Telemetry;

    uint8_t mech_id = 0;
    uint8_t reserved = 0;
    uint8_t holder_id = kHolderNone;
    REG::BitMask<IMech::Status> status{};
    int32_t position_mm = 0;

    [[nodiscard]] bool serialize(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] static bool deserialize(const BIF::CAN::Frame& frame, Telemetry& out) noexcept;
};

using Message = std::variant<Ack,
                             Nack,
                             Heartbeat,
                             Select,
                             Block,
                             GetTelemetry,
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
    case MsgId::Block:
    case MsgId::GetTelemetry:
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
    case MsgId::Block:
    case MsgId::GetTelemetry:
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
    case MsgId::Block:
    case MsgId::GetTelemetry:
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
