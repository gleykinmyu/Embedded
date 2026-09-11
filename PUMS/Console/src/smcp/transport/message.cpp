/**
 * @file message.cpp
 * @brief Сериализация SMCP Packet ↔ BIF::CAN::Frame.
 */

#include "smcp/transport/message.hpp"

namespace smcp {
namespace msg {
namespace {

void store_le16(uint8_t* p, uint16_t v) noexcept
{
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
}

void store_le32(uint8_t* p, uint32_t v) noexcept
{
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
    p[2] = static_cast<uint8_t>(v >> 16);
    p[3] = static_cast<uint8_t>(v >> 24);
}

[[nodiscard]] uint16_t load_le16(const uint8_t* p) noexcept
{
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

[[nodiscard]] uint32_t load_le32(const uint8_t* p) noexcept
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

[[nodiscard]] bool push(BIF::CAN::Frame& frame, uint8_t b) noexcept
{
    if (frame.dlc >= BIF::CAN::kMaxDataLength) {
        return false;
    }
    frame.data[frame.dlc++] = b;
    return true;
}

[[nodiscard]] bool push_le16(BIF::CAN::Frame& frame, uint16_t v) noexcept
{
    uint8_t tmp[2];
    store_le16(tmp, v);
    return push(frame, tmp[0]) && push(frame, tmp[1]);
}

[[nodiscard]] bool push_le32(BIF::CAN::Frame& frame, uint32_t v) noexcept
{
    uint8_t tmp[4];
    store_le32(tmp, v);
    return push(frame, tmp[0]) && push(frame, tmp[1]) && push(frame, tmp[2]) && push(frame, tmp[3]);
}

[[nodiscard]] bool expectDlc(const BIF::CAN::Frame& frame, uint8_t dlc) noexcept
{
    return frame.dlc == dlc;
}

} // namespace

bool Header::serialize(BIF::CAN::Frame& frame) const noexcept
{
    Header hdr = *this;
    if (hdr.prio > kPrioMax) {
        hdr.prio = kPrioMax;
    }

    frame = {};
    frame.id.setExtended(true);
    frame.id.set(helpers::packCanId(hdr));
    frame.dlc = 0;
    return true;
}

bool Header::deserialize(const BIF::CAN::Frame& frame) noexcept
{
    if (!frame.id.extended || frame.dlc > BIF::CAN::kMaxDataLength) {
        return false;
    }

    *this = helpers::unpackCanId(frame.id.get());
    return true;
}

namespace helpers {

Header headerFromCanFrame(const BIF::CAN::Frame& frame) noexcept
{
    Header hdr{};
    (void)hdr.deserialize(frame);
    return hdr;
}

bool toCanFrame(Packet packet, BIF::CAN::Frame& out) noexcept
{
    packet.hdr.msg_id = msgIdOf(packet.body);
    if (!packet.hdr.serialize(out)) {
        return false;
    }
    if (carriesPktId(packet.hdr.msg_id) && !push(out, packet.pkt_id)) {
        return false;
    }
    return std::visit([&](const auto& body) noexcept { return body.serialize(out); }, packet.body);
}

bool fromCanFrame(const BIF::CAN::Frame& frame, Packet& packet) noexcept
{
    if (!packet.hdr.deserialize(frame)) {
        return false;
    }

    packet.pkt_id = 0;
    if (carriesPktId(packet.hdr.msg_id)) {
        if (frame.dlc < 1u) {
            return false;
        }
        packet.pkt_id = frame.data[0];
    }

    switch (packet.hdr.msg_id) {
    case MsgId::Ack: {
        Ack body{};
        if (!Ack::deserialize(frame, body)) {
            return false;
        }
        packet.body = body;
        return true;
    }
    case MsgId::Nack: {
        Nack body{};
        if (!Nack::deserialize(frame, body)) {
            return false;
        }
        packet.body = body;
        return true;
    }
    case MsgId::Heartbeat: {
        Heartbeat body{};
        if (!Heartbeat::deserialize(frame, body)) {
            return false;
        }
        packet.body = body;
        return true;
    }
    case MsgId::Select: {
        Select body{};
        if (!Select::deserialize(frame, body)) {
            return false;
        }
        packet.body = body;
        return true;
    }
    case MsgId::Block: {
        Block body{};
        if (!Block::deserialize(frame, body)) {
            return false;
        }
        packet.body = body;
        return true;
    }
    case MsgId::SetTarget: {
        SetTarget body{};
        if (!SetTarget::deserialize(frame, body)) {
            return false;
        }
        packet.body = body;
        return true;
    }
    case MsgId::Telemetry: {
        Telemetry body{};
        if (!Telemetry::deserialize(frame, body)) {
            return false;
        }
        packet.body = body;
        return true;
    }
    default:
        return false;
    }
}

} // namespace helpers

bool Ack::serialize(BIF::CAN::Frame& /*frame*/) const noexcept
{
    return true; /* pkt_id уже в Packet / helpers::toCanFrame */
}

bool Ack::deserialize(const BIF::CAN::Frame& frame, Ack& /*out*/) noexcept
{
    return expectDlc(frame, 1);
}

bool Nack::serialize(BIF::CAN::Frame& frame) const noexcept
{
    return push(frame, static_cast<uint8_t>(code));
}

bool Nack::deserialize(const BIF::CAN::Frame& frame, Nack& out) noexcept
{
    if (!expectDlc(frame, 2)) {
        return false;
    }
    out.code = static_cast<ErrorCode>(frame.data[1]);
    return true;
}

bool Heartbeat::serialize(BIF::CAN::Frame& /*frame*/) const noexcept
{
    return true;
}

bool Heartbeat::deserialize(const BIF::CAN::Frame& frame, Heartbeat& /*out*/) noexcept
{
    return expectDlc(frame, 0);
}

bool Select::serialize(BIF::CAN::Frame& frame) const noexcept
{
    return push(frame, static_cast<uint8_t>(action)) && push_le32(frame, selection.raw());
}

bool Select::deserialize(const BIF::CAN::Frame& frame, Select& out) noexcept
{
    if (!expectDlc(frame, 6)) {
        return false;
    }
    const auto action = static_cast<Action>(frame.data[1]);
    if (!isValidAction(action)) {
        return false;
    }
    out.action = action;
    out.selection = Selection::from_raw(load_le32(frame.data + 2));
    return true;
}

bool Block::serialize(BIF::CAN::Frame& frame) const noexcept
{
    return push(frame, static_cast<uint8_t>(action)) && push_le32(frame, selection.raw());
}

bool Block::deserialize(const BIF::CAN::Frame& frame, Block& out) noexcept
{
    if (!expectDlc(frame, 6)) {
        return false;
    }
    const auto action = static_cast<Action>(frame.data[1]);
    if (!isValidAction(action)) {
        return false;
    }
    out.action = action;
    out.selection = Selection::from_raw(load_le32(frame.data + 2));
    return true;
}

bool SetTarget::serialize(BIF::CAN::Frame& frame) const noexcept
{
    return push(frame, mech_id) && push_le32(frame, static_cast<uint32_t>(target.target_mm))
        && push_le16(frame, target.speed_mm_s);
}

bool SetTarget::deserialize(const BIF::CAN::Frame& frame, SetTarget& out) noexcept
{
    if (!expectDlc(frame, 8)) {
        return false;
    }
    out.mech_id = frame.data[1];
    out.target.target_mm = static_cast<int32_t>(load_le32(frame.data + 2));
    out.target.speed_mm_s = load_le16(frame.data + 6);
    out.target.accel_mm_s2 = 0;
    return true;
}

bool Telemetry::serialize(BIF::CAN::Frame& frame) const noexcept
{
    return push(frame, mech_id) && push(frame, holder_id)
        && push_le32(frame, static_cast<uint32_t>(position_mm))
        && push(frame, static_cast<uint8_t>(status.raw()));
}

bool Telemetry::deserialize(const BIF::CAN::Frame& frame, Telemetry& out) noexcept
{
    if (!expectDlc(frame, 7)) {
        return false;
    }
    out.mech_id = frame.data[0];
    out.holder_id = frame.data[1];
    out.position_mm = static_cast<int32_t>(load_le32(frame.data + 2));
    out.status = REG::BitMask<IMech::Status>::from_raw(frame.data[6]);
    return true;
}

} // namespace msg
} // namespace smcp
