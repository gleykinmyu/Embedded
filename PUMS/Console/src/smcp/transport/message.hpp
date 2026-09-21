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

/** Служебные MsgId (диапазон 0x00…0x0F). Прикладные — в своих заголовках. */
enum class MsgId : uint8_t {
    Ack       = 0x01,
    Nack      = 0x02,
    Heartbeat = 0x30, /**< Ниже команд: keep-alive не забивает Select/SetTarget. */
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
    }
    return "?";
}

[[nodiscard]] inline const char* cstrMsg(uint8_t id) noexcept
{
    switch (id) {
    case static_cast<uint8_t>(MsgId::Ack): return "Ack";
    case static_cast<uint8_t>(MsgId::Nack): return "Nack";
    case static_cast<uint8_t>(MsgId::Heartbeat): return "Heartbeat";
    default: return "?";
    }
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

[[nodiscard]] inline const char* cstrS(MsgId id) noexcept
{
    switch (id) {
    case MsgId::Ack: return "Ack";
    case MsgId::Nack: return "Nk";
    case MsgId::Heartbeat: return "HB";
    }
    return "?";
}

[[nodiscard]] inline const char* cstrMsgS(uint8_t id) noexcept
{
    switch (id) {
    case static_cast<uint8_t>(MsgId::Ack): return "Ack";
    case static_cast<uint8_t>(MsgId::Nack): return "Nk";
    case static_cast<uint8_t>(MsgId::Heartbeat): return "HB";
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
    [[nodiscard]] bool push_le16(uint16_t v) noexcept;
    [[nodiscard]] bool push_le32(uint32_t v) noexcept;
    [[nodiscard]] uint16_t load_le16(uint8_t off) const noexcept;
    [[nodiscard]] uint32_t load_le32(uint8_t off) const noexcept;
    [[nodiscard]] bool expectDlc(uint8_t want) const noexcept;
};

struct Packet {
    uint8_t src_id = 0;
    uint8_t dst_id = 0;
    uint8_t pkt_id = 0;
    Message msg{};

    /** Кадр адресован этому узлу (или broadcast). */
    [[nodiscard]] constexpr bool isAddressedTo(uint8_t node_id) const noexcept
    {
        return dst_id == node_id || dst_id == kBroadcastId;
    }

    [[nodiscard]] bool pack(BIF::CAN::Frame& frame) const noexcept;
    [[nodiscard]] bool unpack(const BIF::CAN::Frame& frame) noexcept;
};

inline constexpr uint8_t kNackDetailNone = 0xFFu;
/**
 * Ack — DLC=0 (pkt_id в CAN ID).
 */
struct Ack {
    static constexpr uint8_t kId = static_cast<uint8_t>(MsgId::Ack);
    static constexpr bool kNeedsAck = false;
    static constexpr uint16_t kTimeoutMs = 100u;
    static constexpr uint8_t kRetry = 3u;

    void pack(Message& m) const noexcept;
    [[nodiscard]] bool unpack(const Message& m) noexcept;
};

/**
 * Nack — DLC=2
 *   +------+--------+
 *   | code | detail |
 *   +------+--------+
 *    data[0] data[1]   (pkt_id → CAN ID)
 *
 * @a detail — контекст отказа (обычно mech_id); @c kNackDetailNone = нет.
 */
struct Nack {
    static constexpr uint8_t kId = static_cast<uint8_t>(MsgId::Nack);
    static constexpr bool kNeedsAck = false;
    ErrorCode code = ErrorCode::Busy;
    uint8_t detail = kNackDetailNone;

    void pack(Message& m) const noexcept;
    [[nodiscard]] bool unpack(const Message& m) noexcept;
};

/**
 * Heartbeat — DLC=0 (только ID, pkt_id=0).
 */
struct Heartbeat {
    static constexpr uint8_t kId = static_cast<uint8_t>(MsgId::Heartbeat);
    static constexpr bool kNeedsAck = false;
    static constexpr uint16_t kTimeoutMs = 500u;
    /** Сколько интервалов T без RX HB до down. */
    static constexpr uint8_t kMissMax = 3u;

    void pack(Message& m) const noexcept;
    [[nodiscard]] bool unpack(const Message& m) noexcept;
};

/** Свободные хелперы wire/правил (не поля body). */
namespace helpers {

[[nodiscard]] constexpr bool isBroadcastId(uint8_t id) noexcept
{
    return id == kBroadcastId;
}

[[nodiscard]] constexpr bool isTransportMsg(uint8_t id) noexcept
{
    return id == Ack::kId || id == Nack::kId || id == Heartbeat::kId;
}

/**
 * Свой msg_id съели (даже если unpack не удался).
 * false — не этот тип.
 */
template <typename T, typename F>
[[nodiscard]] bool take(const Packet& pkt, F&& fn) noexcept
{
    if (pkt.msg.id != T::kId) {
        return false;
    }
    T body{};
    if (body.unpack(pkt.msg)) {
        fn(body);
    }
    return true;
}

} // namespace helpers

} // namespace msg
} // namespace smcp
