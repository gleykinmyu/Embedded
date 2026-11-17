/**
 * @file new_smcp/packet.hpp
 * @brief smcp::Packet — Frame для comm::Node / ILink.
 *
 * CAN ID (29 bit): msg[28:22] | dst[21:14] | src[13:6] | txn[5:0]
 * data[0..7] — Message payload (txn в ID, не в data).
 */

#pragma once

#include <cstdint>

#include "comm/session.hpp"
#include "smcp/new_smcp/addr.hpp"
#include "smcp/new_smcp/message.hpp"

namespace smcp {

inline constexpr unsigned kCanIdTxnPos = 0;
inline constexpr unsigned kCanIdSrcPos = 6;
inline constexpr unsigned kCanIdDstPos = 14;
inline constexpr unsigned kCanIdMsgPos = 22;

struct Packet {
    addr_t src{};
    addr_t dst{};
    uint8_t txn = 0; ///< 0 — вне транзакции
    Message msg{};

    [[nodiscard]] uint32_t canId() const noexcept
    {
        const uint32_t mid = static_cast<uint32_t>(static_cast<uint8_t>(msg.id) & kMsgIdMax);
        const uint32_t d = static_cast<uint32_t>(dst.v);
        const uint32_t s = static_cast<uint32_t>(src.v);
        const uint32_t t = static_cast<uint32_t>(txn & kTxnIdMax);
        return (mid << kCanIdMsgPos) | (d << kCanIdDstPos) | (s << kCanIdSrcPos)
            | (t << kCanIdTxnPos);
    }

    bool fromCanId(uint32_t can_id, const uint8_t* data, uint8_t dlc) noexcept
    {
        msg.id = static_cast<MsgId>((can_id >> kCanIdMsgPos) & kMsgIdMax);
        dst.v = static_cast<uint8_t>((can_id >> kCanIdDstPos) & 0xFFu);
        src.v = static_cast<uint8_t>((can_id >> kCanIdSrcPos) & 0xFFu);
        txn = static_cast<uint8_t>((can_id >> kCanIdTxnPos) & kTxnIdMax);
        msg.setPayload(data, dlc);
        return true;
    }
};

using SessionT = comm::Session<Packet>;

} // namespace smcp
