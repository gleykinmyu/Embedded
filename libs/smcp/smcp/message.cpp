/**
 * @file message.cpp
 * @brief Конверт SMCP Packet ↔ BIF::CAN::Frame; служебные PDU.
 */

#include "smcp/message.hpp"

#include <cstring>

namespace smcp {

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

bool Message::pushU16(uint16_t v) noexcept
{
    if (static_cast<unsigned>(dlc) + 2u > kMaxDlc) {
        return false;
    }
    data[dlc] = static_cast<uint8_t>(v);
    data[dlc + 1u] = static_cast<uint8_t>(v >> 8);
    dlc = static_cast<uint8_t>(dlc + 2u);
    return true;
}

bool Message::pushU32(uint32_t v) noexcept
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

uint16_t Message::loadU16(uint8_t off) const noexcept
{
    return static_cast<uint16_t>(data[off] | (static_cast<uint16_t>(data[off + 1u]) << 8));
}

uint32_t Message::loadU32(uint8_t off) const noexcept
{
    return static_cast<uint32_t>(data[off]) | (static_cast<uint32_t>(data[off + 1u]) << 8)
         | (static_cast<uint32_t>(data[off + 2u]) << 16)
         | (static_cast<uint32_t>(data[off + 3u]) << 24);
}

bool Message::isTransport() const noexcept
{
    return id == static_cast<uint8_t>(msg::Id::Ack)
        || id == static_cast<uint8_t>(msg::Id::Nack)
        || id == static_cast<uint8_t>(msg::Id::Fault)
        || id == static_cast<uint8_t>(msg::Id::Heartbeat);
}

bool Packet::isAddressedTo(uint8_t node_id) const noexcept
{
    return dst_id == node_id || dst_id == Packet::kBroadcastId;
}

bool Packet::pack(BIF::CAN::Frame& frame) const noexcept
{
    if (msg.dlc > Message::kMaxDlc) {
        return false;
    }
    const uint32_t id_msg = static_cast<uint32_t>(msg.id & protocol::kMsgIdMax);
    const uint32_t id_pkt = static_cast<uint32_t>(pkt_id & protocol::kPktIdMax);
    const uint32_t can_id = (id_msg << protocol::kCanIdMsgPos) | (static_cast<uint32_t>(dst_id) << protocol::kCanIdDstPos)
         | (static_cast<uint32_t>(src_id) << protocol::kCanIdSrcPos) | id_pkt;

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
    pkt_id = static_cast<uint8_t>(can_id & protocol::kPktIdMax);
    src_id = static_cast<uint8_t>((can_id >> protocol::kCanIdSrcPos) & 0xFFu);
    dst_id = static_cast<uint8_t>((can_id >> protocol::kCanIdDstPos) & 0xFFu);
    msg.id = static_cast<uint8_t>((can_id >> protocol::kCanIdMsgPos) & protocol::kMsgIdMax);

    msg.setPayload(frame.data, frame.dlc);
    return true;
}

namespace msg {

bool Ack::pack(Message& m) const noexcept
{
    (void)m;
    return true;
}

bool Ack::unpack(const Message& m) noexcept
{
    return m.dlc == 0;
}

bool Nack::pack(Message& m) const noexcept
{
    return m.push(error) && m.pushU32(detail);
}

bool Nack::unpack(const Message& m) noexcept
{
    if (m.dlc != 5) {
        return false;
    }
    error = m.data[0];
    detail = m.loadU32(1);
    return true;
}

bool Fault::pack(Message& m) const noexcept
{
    return m.push(error) && m.pushU32(detail);
}

bool Fault::unpack(const Message& m) noexcept
{
    if (m.dlc != 5) {
        return false;
    }
    error = m.data[0];
    detail = m.loadU32(1);
    return true;
}

bool Heartbeat::pack(Message& m) const noexcept
{
    (void)m;
    return true;
}

bool Heartbeat::unpack(const Message& m) noexcept
{
    return m.dlc == 0;
}

} // namespace msg
} // namespace smcp
