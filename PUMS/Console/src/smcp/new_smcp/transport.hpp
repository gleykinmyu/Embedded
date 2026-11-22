/**
 * @file new_smcp/transport.hpp
 * @brief smcp::MsgId + Message + Frame — один enum на весь узел (транспорт + northbound).
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

#include "comm/message.hpp"
#include "comm/session.hpp"
#include "smcp/new_smcp/addr.hpp"

namespace smcp {

inline constexpr uint8_t kMsgIdBits = 7u;
inline constexpr uint8_t kMsgIdMax = (1u << kMsgIdBits) - 1u;
inline constexpr uint8_t kTxnIdBits = 6u;
inline constexpr uint8_t kTxnIdMax = (1u << kTxnIdBits) - 1u;
inline constexpr uint8_t kIndexAll = 0xFFu;

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
    SeqData = 0x33,

    Telemetry = 0x40,
};

/** Тело SMCP-кадра: MsgId + 8 байт payload. */
using Frame = comm::Frame<addr_t, 8u, MsgId>;
using Message = comm::Message<8u, MsgId>;

using SessionT = comm::Session<Frame>;

} // namespace smcp

/**
 * Demux PDU в onPacket(const Frame& pkt): id совпал и unpack удался — body в блоке.
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
    if (Type body{}; pkt.id == Type::kId && body.unpack(pkt))
