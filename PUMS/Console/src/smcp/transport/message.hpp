/**
 * @file message.hpp
 * @brief Транспорт SMCP: конверт CAN (ID + payload), служебные PDU.
 *
 * Правила: PROTOCOL.md
 *
 * CAN ID (29 bit):
 *   msg_id[28:22] | dst[21:14] | src[13:6] | pkt_id[5:0]
 * data[0..7] — непрозрачный payload (pkt_id в ID, не в data).
 * Class C/D/E: pkt_id = 0.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "ican.hpp"

namespace smcp {
namespace msg {

inline constexpr uint8_t kBroadcastId = 0xFFu;

inline constexpr uint8_t kMsgIdBits = 7u;
inline constexpr uint8_t kMsgIdMax = (1u << kMsgIdBits) - 1u;
inline constexpr uint8_t kPktIdBits = 6u;
inline constexpr uint8_t kPktIdMax = (1u << kPktIdBits) - 1u;

inline constexpr unsigned kCanIdPktPos = 0;
inline constexpr unsigned kCanIdSrcPos = 6;
inline constexpr unsigned kCanIdDstPos = 14;
inline constexpr unsigned kCanIdMsgPos = 22;

/** Служебные TMsgId (диапазон 0x00…0x0F). Прикладные — в своих заголовках. */
enum class TMsgId : uint8_t {
    Ack       = 0x01,
    Nack      = 0x02,
    Heartbeat = 0x30, /**< Ниже команд: keep-alive не забивает Select/SetTarget. */
};

[[nodiscard]] inline const char* cstr(TMsgId id) noexcept
{
    switch (id) {
    case TMsgId::Ack: return "Ack";
    case TMsgId::Nack: return "Nack";
    case TMsgId::Heartbeat: return "Heartbeat";
    }
    return "?";
}

[[nodiscard]] inline const char* cstrMsg(uint8_t id) noexcept
{
    switch (id) {
    case static_cast<uint8_t>(TMsgId::Ack): return "Ack";
    case static_cast<uint8_t>(TMsgId::Nack): return "Nack";
    case static_cast<uint8_t>(TMsgId::Heartbeat): return "Heartbeat";
    default: return "?";
    }
}

#if defined(SMCP_TRACE_SHORT)
[[nodiscard]] inline const char* cstrS(TMsgId id) noexcept
{
    switch (id) {
    case TMsgId::Ack: return "Ack";
    case TMsgId::Nack: return "Nk";
    case TMsgId::Heartbeat: return "HB";
    }
    return "?";
}

[[nodiscard]] inline const char* cstrMsgS(uint8_t id) noexcept
{
    switch (id) {
    case static_cast<uint8_t>(TMsgId::Ack): return "Ack";
    case static_cast<uint8_t>(TMsgId::Nack): return "Nk";
    case static_cast<uint8_t>(TMsgId::Heartbeat): return "HB";
    default: return "?";
    }
}
#endif

/**
 *   CAN ID (29 bit, IDE=1):
 *   +---------+--------+--------+---------+
 *   | msg_id  |  dst   |  src   | pkt_id  |
 *   |  7 bit  | 8 bit  | 8 bit  |  6 bit  |
 *   +---------+--------+--------+---------+
 *   [28:22]    [21:14]  [13:6]   [5:0]
 *
 * Message — тип + payload. Packet — кадр: адреса, pkt_id, Message.
 */
struct Message {
    static constexpr uint8_t kMaxDlc = 8u;

    uint8_t id = 0;
    uint8_t dlc = 0;
    uint8_t data[8]{};

    void setPayload(const uint8_t* src, uint8_t n) noexcept;
    [[nodiscard]] bool push(uint8_t b) noexcept;
    [[nodiscard]] bool pushU16(uint16_t v) noexcept;
    [[nodiscard]] bool pushU32(uint32_t v) noexcept;
    [[nodiscard]] uint16_t loadU16(uint8_t off) const noexcept;
    [[nodiscard]] uint32_t loadU32(uint8_t off) const noexcept;

    [[nodiscard]] constexpr bool isTransport() const noexcept
    {
        return id == static_cast<uint8_t>(TMsgId::Ack)
            || id == static_cast<uint8_t>(TMsgId::Nack)
            || id == static_cast<uint8_t>(TMsgId::Heartbeat);
    }
};

struct Packet {
    uint8_t src_id = 0;
    uint8_t dst_id = 0;
    uint8_t pkt_id = 0;
    Message msg{};

    /** Кадр адресован этому узлу (или broadcast). */
    [[nodiscard]] bool isAddressedTo(uint8_t node_id) const noexcept;
    [[nodiscard]] bool pack(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] bool unpack(const BIF::CAN::Frame& frame) noexcept;
};

/**
 * Ack — DLC=0 (pkt_id запроса в CAN ID).
 */
struct Ack {
    static constexpr uint8_t kId = static_cast<uint8_t>(TMsgId::Ack);
    static constexpr bool kNeedsAck = false;
    static constexpr uint16_t kTimeoutMs = 100u;
    static constexpr uint8_t kRetry = 3u;

    [[nodiscard]] bool pack(Message& m) const noexcept;
    [[nodiscard]] bool unpack(const Message& m) noexcept;
};

/**
 * Nack — DLC=2
 *   +-------+--------+
 *   | error | detail |
 *   +-------+--------+
 *    data[0]  data[1]   (pkt_id запроса → CAN ID)
 *
 * @a error — код отказа (вышестоящий протокол). @a detail — контекст
 * (обычно mech_id); @c kDetailNone = нет.
 * @c kTimeout — локальный abort Ack (не с шины).
 */
struct Nack {
    static constexpr uint8_t kId = static_cast<uint8_t>(TMsgId::Nack);
    static constexpr bool kNeedsAck = false;
    static constexpr uint8_t kTimeout = 0x08u;
    static constexpr uint8_t kDetailNone = 0xFFu;

    uint8_t error = 0;
    uint8_t detail = kDetailNone;

    [[nodiscard]] bool pack(Message& m) const noexcept;
    [[nodiscard]] bool unpack(const Message& m) noexcept;
};

/**
 * Heartbeat — DLC=0 (только ID, pkt_id=0).
 */
struct Heartbeat {
    static constexpr uint8_t kId = static_cast<uint8_t>(TMsgId::Heartbeat);
    static constexpr bool kNeedsAck = false;
    static constexpr uint16_t kTimeoutMs = 500u;
    /** Сколько интервалов T без RX HB до down. */
    static constexpr uint8_t kMissMax = 3u;

    [[nodiscard]] bool pack(Message& m) const noexcept;
    [[nodiscard]] bool unpack(const Message& m) noexcept;
};

[[nodiscard]] constexpr bool isBroadcastId(uint8_t id) noexcept
{
    return id == kBroadcastId;
}

} // namespace msg
} // namespace smcp

/**
 * Demux PDU в `onPacket(const Packet& pkt)`: id совпал и unpack удался — `body` в блоке.
 * Цепочка: `else SMCP_IF_MSG` (макрос сам `if`). Ждёт имя `pkt` в скоупе.
 *
 *   SMCP_IF_MSG(msg::Select) {
 *       onSelect(body, pkt.pkt_id);
 *   } else SMCP_IF_MSG(msg::SetTarget) {
 *       onSetTarget(body, pkt.pkt_id);
 *   } else {
 *       return false;
 *   }
 *   return true;
 */
#define SMCP_IF_MSG(Type) \
    if (Type body{}; pkt.msg.id == Type::kId && body.unpack(pkt.msg))
