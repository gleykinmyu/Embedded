/**
 * @file new_smcp/packet.hpp
 * @brief smcp::Packet — Frame для comm::Node / ILink.
 *
 * CAN ID (29 bit): msg[28:22] | dst[21:14] | src[13:6] | txn[5:0]
 * data[0..7] — proto::msg::Header payload (txn в ID, не в data).
 */

#pragma once

#include <cstdint>

#include "comm/session.hpp"
#include "smcp/new_smcp/addr.hpp"
#include "smcp/new_smcp/proto/msg/frame.hpp"

/*
 * Extended CAN ID, 29 бит (IDE=1). Бит 28 — старший: меньший msg выигрывает арбитраж.
 *
 *  28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
 * +--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+
 * |m6|m5|m4|m3|m2|m1|m0|d7|d6|d5|d4|d3|d2|d1|d0|s7|s6|s5|s4|s3|s2|s1|s0|t5|t4|t3|t2|t1|t0|
 * +--------------------+-----------------------+-----------------------+-----------------+
 * |        msg         |          dst          |          src          |       txn       |
 * |      [28:22]       |        [21:14]        |         [13:6]        |      [5:0]      |
 * |       7 bit        |         8 bit         |         8 bit         |      6 bit      |
 * +--------------------+-----------------------+-----------------------+-----------------+
 *
 * msg  proto::msg::Id. 0x01…0x7F; меньший id — выше приоритет на шине.
 * dst  addr_t, кому.  0x00 none, 0x01…0x0F console, 0x10…0xEF server, 0xFF broadcast.
 * src  addr_t, от кого. Те же диапазоны.
 * txn  0 — вне транзакции, 1…0x3F — id транзакции (proto::msg::kTxnIdMax).
 * data[0..7] — payload proto::msg::Header; txn в ID, не в data.
 */

namespace smcp {



inline constexpr unsigned kCanIdTxnPos = 0;
inline constexpr unsigned kCanIdSrcPos = 6;
inline constexpr unsigned kCanIdDstPos = 14;
inline constexpr unsigned kCanIdMsgPos = 22;
inline constexpr uint8_t kMsgIdMax = 0x7Fu;

struct Packet {
    addr_t src{};
    addr_t dst{};
    uint8_t txn = 0; ///< 0 — вне транзакции
    proto::msg::Header msg{};

    [[nodiscard]] uint32_t canId() const noexcept
    {
        const uint32_t mid = static_cast<uint32_t>(static_cast<uint8_t>(msg.id) & kMsgIdMax);
        const uint32_t d = static_cast<uint32_t>(dst.v);
        const uint32_t s = static_cast<uint32_t>(src.v);
        const uint32_t t = static_cast<uint32_t>(txn & proto::msg::kTxnIdMax);
        return (mid << kCanIdMsgPos) | (d << kCanIdDstPos) | (s << kCanIdSrcPos)
            | (t << kCanIdTxnPos);
    }

    bool fromCanId(uint32_t can_id, const uint8_t* data, uint8_t dlc) noexcept
    {
        msg.id = static_cast<proto::msg::Id>((can_id >> kCanIdMsgPos) & kMsgIdMax);
        dst.v = static_cast<uint8_t>((can_id >> kCanIdDstPos) & 0xFFu);
        src.v = static_cast<uint8_t>((can_id >> kCanIdSrcPos) & 0xFFu);
        txn = static_cast<uint8_t>((can_id >> kCanIdTxnPos) & proto::msg::kTxnIdMax);
        if (dlc > proto::msg::Header::kMaxDlc) {
            dlc = proto::msg::Header::kMaxDlc;
        }
        msg.payload.clear();
        if (data != nullptr && dlc != 0u) {
            comm::PayloadWriter{msg.payload}.push(data, dlc);
        }
        return true;
    }
};

using SessionT = comm::Session<Packet>;

} // namespace smcp
