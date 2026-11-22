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

#include <cstdint>

#include "comm/payload.hpp"
#include "smcp/new_smcp/proto/base/frame.hpp"

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

/** Кадр: id = MsgId, payload — байты data CAN. */
struct Message {
    using FramePayload = decltype(FrameBase{}.payload);
    /** Байт в data кадра: из FrameBase уходит то, что не поместилось в CAN ID. */
    static constexpr uint8_t kMaxDlc = static_cast<uint8_t>(
        FramePayload::kLenMax - FramePayload::kLenMin);

    MsgId id{};
    comm::Payload<0, kMaxDlc> payload{};
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
