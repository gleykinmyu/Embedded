/**
 * @file message.cpp
 * @brief Конверт SMCP Packet ↔ BIF::CAN::Frame; служебные PDU.
 */

#include "smcp/transport/message.hpp"

#include <cstring>

namespace smcp {
namespace msg {

void Message::setPayload(const uint8_t* src, uint8_t n) noexcept
{
    dlc = (n > kMaxDlc) ? kMaxDlc : n;
    if (src != nullptr && dlc > 0u) {
        std::memcpy(data, src, dlc);
    }
    for (uint8_t i = dlc; i < kMaxDlc; ++i) {
        data[i] = 0;
    }
}

bool Message::push(uint8_t b) noexcept
{
    if (dlc >= kMaxDlc) {
        return false;
    }
    data[dlc++] = b;
    return true;
}

bool Message::push_le16(uint16_t v) noexcept
{
    if (static_cast<unsigned>(dlc) + 2u > kMaxDlc) {
        return false;
    }
    data[dlc] = static_cast<uint8_t>(v);
    data[dlc + 1u] = static_cast<uint8_t>(v >> 8);
    dlc = static_cast<uint8_t>(dlc + 2u);
    return true;
}

bool Message::push_le32(uint32_t v) noexcept
{
    if (static_cast<unsigned>(dlc) + 4u > kMaxDlc) {
        return false;
    }
    data[dlc] = static_cast<uint8_t>(v);
    data[dlc + 1u] = static_cast<uint8_t>(v >> 8);
    data[dlc + 2u] = static_cast<uint8_t>(v >> 16);
    data[dlc + 3u] = static_cast<uint8_t>(v >> 24);
    dlc = static_cast<uint8_t>(dlc + 4u);
    return true;
}

uint16_t Message::load_le16(uint8_t off) const noexcept
{
    return static_cast<uint16_t>(data[off] | (static_cast<uint16_t>(data[off + 1u]) << 8));
}

uint32_t Message::load_le32(uint8_t off) const noexcept
{
    return static_cast<uint32_t>(data[off]) | (static_cast<uint32_t>(data[off + 1u]) << 8)
         | (static_cast<uint32_t>(data[off + 2u]) << 16)
         | (static_cast<uint32_t>(data[off + 3u]) << 24);
}

bool Message::expectDlc(uint8_t want) const noexcept
{
    return dlc == want;
}

bool Packet::pack(BIF::CAN::Frame& frame) const noexcept
{
    if (msg.dlc > Message::kMaxDlc) {
        return false;
    }
    const uint32_t id_msg = static_cast<uint32_t>(msg.id & kMsgIdMax);
    const uint32_t id_pkt = static_cast<uint32_t>(pkt_id & kPktIdMax);
    const uint32_t can_id = (id_msg << kCanIdMsgPos) | (static_cast<uint32_t>(dst_id) << kCanIdDstPos)
         | (static_cast<uint32_t>(src_id) << kCanIdSrcPos) | id_pkt;

    frame = {};
    frame.id.setExtended(true);
    frame.id.set(can_id);
    frame.dlc = msg.dlc;
    if (msg.dlc > 0u) {
        std::memcpy(frame.data, msg.data, msg.dlc);
    }
    return true;
}

bool Packet::unpack(const BIF::CAN::Frame& frame) noexcept
{
    if (!frame.id.extended || frame.dlc > BIF::CAN::kMaxDataLength) {
        return false;
    }

    const uint32_t can_id = frame.id.get();
    pkt_id = static_cast<uint8_t>(can_id & kPktIdMax);
    src_id = static_cast<uint8_t>((can_id >> kCanIdSrcPos) & 0xFFu);
    dst_id = static_cast<uint8_t>((can_id >> kCanIdDstPos) & 0xFFu);
    msg.id = static_cast<uint8_t>((can_id >> kCanIdMsgPos) & kMsgIdMax);

    msg.setPayload(frame.data, frame.dlc);
    return true;
}

void Ack::pack(Message& m) const noexcept
{
    (void)m;
}

bool Ack::unpack(const Message& m) noexcept
{
    return m.expectDlc(0);
}

void Nack::pack(Message& m) const noexcept
{
    (void)m.push(static_cast<uint8_t>(code));
    (void)m.push(detail);
}

bool Nack::unpack(const Message& m) noexcept
{
    if (!m.expectDlc(2)) {
        return false;
    }
    code = static_cast<ErrorCode>(m.data[0]);
    detail = m.data[1];
    return true;
}

void Heartbeat::pack(Message& m) const noexcept
{
    (void)m;
}

bool Heartbeat::unpack(const Message& m) noexcept
{
    return m.expectDlc(0);
}

} // namespace msg
} // namespace smcp
