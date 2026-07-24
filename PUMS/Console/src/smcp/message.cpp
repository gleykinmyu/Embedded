/**
 * @file message.cpp
 * @brief Сериализация SMCP Packet ↔ BIF::CAN::Frame.
 */

#include "message.hpp"

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

[[nodiscard]] bool expectBody(const BIF::CAN::Frame& frame, uint8_t body_len) noexcept
{
    return frame.dlc == static_cast<uint8_t>(kPayloadOffset + body_len);
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
    frame.id.set(packCanId(hdr));
    frame.dlc = 0;
    return push(frame, hdr.pkt_id);
}

bool Header::deserialize(const BIF::CAN::Frame& frame) noexcept
{
    if (!frame.id.extended || frame.dlc < 1 || frame.dlc > BIF::CAN::kMaxDataLength) {
        return false;
    }

    *this = unpackCanId(frame.id.get());
    pkt_id = frame.data[kPktIdOffset];
    return true;
}

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
    return std::visit([&](const auto& body) noexcept { return body.serialize(out); }, packet.body);
}

bool fromCanFrame(const BIF::CAN::Frame& frame, Packet& packet) noexcept
{
    if (!packet.hdr.deserialize(frame)) {
        return false;
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
    case MsgId::SysInfo: {
        SysInfo body{};
        if (!SysInfo::deserialize(frame, body)) {
            return false;
        }
        packet.body = body;
        return true;
    }
    case MsgId::GetLog: {
        GetLog body{};
        if (!GetLog::deserialize(frame, body)) {
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
    case MsgId::ResetFault: {
        ResetFault body{};
        if (!ResetFault::deserialize(frame, body)) {
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
    case MsgId::SetConfig: {
        SetConfig body{};
        if (!SetConfig::deserialize(frame, body)) {
            return false;
        }
        packet.body = body;
        return true;
    }
    case MsgId::GetConfig: {
        GetConfig body{};
        if (!GetConfig::deserialize(frame, body)) {
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
    case MsgId::CriticalErr: {
        CriticalErr body{};
        if (!CriticalErr::deserialize(frame, body)) {
            return false;
        }
        packet.body = body;
        return true;
    }
    default:
        return false;
    }
}

// --- body serialize / deserialize (payload с data[kPayloadOffset]) ---

bool Ack::serialize(BIF::CAN::Frame& /*frame*/) const noexcept
{
    return true;
}

bool Ack::deserialize(const BIF::CAN::Frame& frame, Ack& /*out*/) noexcept
{
    return expectBody(frame, 0);
}

bool Nack::serialize(BIF::CAN::Frame& frame) const noexcept
{
    return push(frame, static_cast<uint8_t>(code));
}

bool Nack::deserialize(const BIF::CAN::Frame& frame, Nack& out) noexcept
{
    if (!expectBody(frame, 1)) {
        return false;
    }
    out.code = static_cast<ErrorCode>(frame.data[kPayloadOffset]);
    return true;
}

bool Heartbeat::serialize(BIF::CAN::Frame& /*frame*/) const noexcept
{
    return true;
}

bool Heartbeat::deserialize(const BIF::CAN::Frame& frame, Heartbeat& /*out*/) noexcept
{
    return expectBody(frame, 0);
}

bool SysInfo::serialize(BIF::CAN::Frame& frame) const noexcept
{
    return push_le16(frame, fw_version) && push(frame, master_status) && push(frame, checker_status);
}

bool SysInfo::deserialize(const BIF::CAN::Frame& frame, SysInfo& out) noexcept
{
    if (!expectBody(frame, 4)) {
        return false;
    }
    const uint8_t* p = frame.data + kPayloadOffset;
    out.fw_version = load_le16(p);
    out.master_status = p[2];
    out.checker_status = p[3];
    return true;
}

bool GetLog::serialize(BIF::CAN::Frame& frame) const noexcept
{
    return push_le32(frame, offset) && push_le16(frame, count);
}

bool GetLog::deserialize(const BIF::CAN::Frame& frame, GetLog& out) noexcept
{
    if (!expectBody(frame, 6)) {
        return false;
    }
    const uint8_t* p = frame.data + kPayloadOffset;
    out.offset = load_le32(p);
    out.count = load_le16(p + 4);
    return true;
}

bool Select::serialize(BIF::CAN::Frame& frame) const noexcept
{
    return push(frame, static_cast<uint8_t>(action)) && push_le32(frame, selection.raw());
}

bool Select::deserialize(const BIF::CAN::Frame& frame, Select& out) noexcept
{
    if (!expectBody(frame, 5)) {
        return false;
    }
    const uint8_t* p = frame.data + kPayloadOffset;
    out.action = static_cast<Action>(p[0]);
    out.selection = Selection::from_raw(load_le32(p + 1));
    return true;
}

bool ResetFault::serialize(BIF::CAN::Frame& frame) const noexcept
{
    return push(frame, mech_id);
}

bool ResetFault::deserialize(const BIF::CAN::Frame& frame, ResetFault& out) noexcept
{
    if (!expectBody(frame, 1)) {
        return false;
    }
    out.mech_id = frame.data[kPayloadOffset];
    return true;
}

bool SetTarget::serialize(BIF::CAN::Frame& frame) const noexcept
{
    return push(frame, mech_id) && push_le32(frame, static_cast<uint32_t>(target.target_mm))
        && push_le16(frame, target.speed_mm_s);
}

bool SetTarget::deserialize(const BIF::CAN::Frame& frame, SetTarget& out) noexcept
{
    if (!expectBody(frame, 7)) {
        return false;
    }
    const uint8_t* p = frame.data + kPayloadOffset;
    out.mech_id = p[0];
    out.target.target_mm = static_cast<int32_t>(load_le32(p + 1));
    out.target.speed_mm_s = load_le16(p + 5);
    out.target.accel_mm_s2 = 0;
    return true;
}

bool SetConfig::serialize(BIF::CAN::Frame& frame) const noexcept
{
    return push(frame, mech_id);
}

bool SetConfig::deserialize(const BIF::CAN::Frame& frame, SetConfig& out) noexcept
{
    if (!expectBody(frame, 1)) {
        return false;
    }
    out.mech_id = frame.data[kPayloadOffset];
    return true;
}

bool GetConfig::serialize(BIF::CAN::Frame& frame) const noexcept
{
    return push(frame, mech_id);
}

bool GetConfig::deserialize(const BIF::CAN::Frame& frame, GetConfig& out) noexcept
{
    if (!expectBody(frame, 1)) {
        return false;
    }
    out.mech_id = frame.data[kPayloadOffset];
    return true;
}

bool Telemetry::serialize(BIF::CAN::Frame& frame) const noexcept
{
    return push(frame, mech_id) && push(frame, select_owner_id)
        && push_le32(frame, static_cast<uint32_t>(position_mm))
        && push(frame, static_cast<uint8_t>(status.raw()));
}

bool Telemetry::deserialize(const BIF::CAN::Frame& frame, Telemetry& out) noexcept
{
    if (!expectBody(frame, 7)) {
        return false;
    }
    const uint8_t* p = frame.data + kPayloadOffset;
    out.mech_id = p[0];
    out.select_owner_id = p[1];
    out.position_mm = static_cast<int32_t>(load_le32(p + 2));
    out.status = REG::BitMask<IMech::Status>::from_raw(p[6]);
    return true;
}

bool CriticalErr::serialize(BIF::CAN::Frame& frame) const noexcept
{
    return push(frame, static_cast<uint8_t>(code)) && push(frame, detail);
}

bool CriticalErr::deserialize(const BIF::CAN::Frame& frame, CriticalErr& out) noexcept
{
    if (!expectBody(frame, 2)) {
        return false;
    }
    const uint8_t* p = frame.data + kPayloadOffset;
    out.code = static_cast<ErrorCode>(p[0]);
    out.detail = p[1];
    return true;
}

} // namespace msg
} // namespace smcp
