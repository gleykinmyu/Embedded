/**
 * @file new_smcp/message.hpp
 * @brief smcp::MsgId + Message — один enum на весь узел (транспорт + northbound).
 *
 * Нумерация (меньший id — выше приоритет на шине):
 *   Ack/Nack/Fault  0x01…
 *   команды         0x10…0x2F
 *   HB              0x30
 *   config Get/Set  0x31…0x34
 *   Telemetry       0x40…
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace smcp {

inline constexpr uint8_t kMsgIdBits = 7u;
inline constexpr uint8_t kMsgIdMax = (1u << kMsgIdBits) - 1u;
inline constexpr uint8_t kTxnIdBits = 6u;
inline constexpr uint8_t kTxnIdMax = (1u << kTxnIdBits) - 1u;
inline constexpr uint8_t kIndexAll = 0xFFu;
inline constexpr uint8_t kInlineMax = 6u; ///< 8 − (tag|index) или 8 − (seq u16)

/** Единый идентификатор PDU на узле. */
enum class MsgId : uint8_t {
    Ack = 0x01,
    Nack = 0x02,
    Fault = 0x03,

    Select = 0x10,
    Block = 0x11,

    SetTarget = 0x20,
    GetTelemetry = 0x21,
    ResetFault = 0x22,
    ServiceAccess = 0x23,

    Heartbeat = 0x30,
    Get = 0x31,
    Set = 0x32,
    DataBegin = 0x33,
    Data = 0x34,

    Telemetry = 0x40,
};

/** Payload кадра (до 8 байт). id = MsgId. */
struct Message {
    static constexpr uint8_t kMaxDlc = 8u;

    MsgId id{};
    uint8_t dlc = 0;
    uint8_t data[kMaxDlc]{};

    void clear() noexcept
    {
        id = MsgId{};
        dlc = 0;
        std::memset(data, 0, sizeof(data));
    }

    void setPayload(const uint8_t* src, uint8_t n) noexcept
    {
        if (src == nullptr) {
            n = 0;
        }
        if (n > kMaxDlc) {
            n = kMaxDlc;
        }
        dlc = n;
        if (n != 0u) {
            std::memcpy(data, src, n);
        }
    }

    void push(uint8_t b) noexcept
    {
        assert(dlc < kMaxDlc);
        if (dlc >= kMaxDlc) {
            return;
        }
        data[dlc++] = b;
    }

    /** enum class : uint8_t — тот же байт на проводе. */
    template <typename E>
    auto push(E v) noexcept -> std::enable_if_t<
        std::is_enum_v<E> && std::is_same_v<std::underlying_type_t<E>, uint8_t>>
    {
        push(static_cast<uint8_t>(v));
    }

    void pushU16(uint16_t v) noexcept
    {
        push(static_cast<uint8_t>(v));
        push(static_cast<uint8_t>(v >> 8));
    }

    void pushU32(uint32_t v) noexcept
    {
        pushU16(static_cast<uint16_t>(v));
        pushU16(static_cast<uint16_t>(v >> 16));
    }

    [[nodiscard]] uint16_t loadU16(uint8_t off) const noexcept
    {
        if (off + 1u >= dlc) {
            return 0;
        }
        return static_cast<uint16_t>(data[off] | (static_cast<uint16_t>(data[off + 1u]) << 8));
    }

    [[nodiscard]] uint32_t loadU32(uint8_t off) const noexcept
    {
        return static_cast<uint32_t>(loadU16(off))
            | (static_cast<uint32_t>(loadU16(static_cast<uint8_t>(off + 2u))) << 16);
    }
};

} // namespace smcp

/**
 * Demux PDU в onPacket(const Packet& pkt): id совпал и unpack удался — body в блоке.
 * Цепочка: else SMCP_IF_MSG (макрос сам if). Ждёт имя pkt в скоупе.
 *
 *   SMCP_IF_MSG(Fault) {
 *       ...
 *   } else SMCP_IF_MSG(Ack) {
 *       ...
 *   }
 *   return false;
 */
#define SMCP_IF_MSG(Type) \
    if (Type body{}; pkt.msg.id == Type::kId && body.unpack(pkt.msg))
