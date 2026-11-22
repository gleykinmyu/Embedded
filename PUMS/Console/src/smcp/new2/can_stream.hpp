/**
 * @file smcp/new2/can_stream.hpp
 * @brief IFrameStream<Frame> над BIF::CAN::ICAN. ILink нет.
 *
 * ID: prio[28:27] | dst[26:20] | src[19:13] | proto[12] | pl0[11:8] | pl1[7:0]
 * data[0..7] = payload[2..9]
 */
#pragma once

#include <cstring>

#include "ican.hpp"

#ifndef SMCP2_CAN
#define SMCP2_CAN
#endif
#include "smcp/new2/proto/base.hpp"

static_assert(kPrioMax == 0x03u, "define SMCP2_CAN before base.hpp");

namespace smcp2 {

inline constexpr unsigned kPl1Pos = 0u;
inline constexpr unsigned kPl0Pos = 8u;
inline constexpr unsigned kProtoPos = 12u;
inline constexpr unsigned kSrcPos = 13u;
inline constexpr unsigned kDstPos = 20u;
inline constexpr unsigned kPrioPos = 27u;

inline constexpr uint32_t kAddrMsk = (1u << 7) - 1u;
inline constexpr uint32_t kPrioMsk = kPrioMax;
inline constexpr uint32_t kProtoMsk = kProtoMax;
inline constexpr uint32_t kPlHiMsk = 0x0Fu;
inline constexpr std::size_t kIdPl = 2u;

[[nodiscard]] inline uint32_t packId(const BaseHdr& h, uint8_t pl0, uint8_t pl1) noexcept
{
    const uint32_t prio = static_cast<uint32_t>(h.prio) & kPrioMsk;
    const uint32_t dst = static_cast<uint32_t>(h.dst.v) & kAddrMsk;
    const uint32_t src = static_cast<uint32_t>(h.src.v) & kAddrMsk;
    const uint32_t proto = static_cast<uint32_t>(h.proto) & kProtoMsk;
    const uint32_t hi = static_cast<uint32_t>(pl0) & kPlHiMsk;
    const uint32_t lo = static_cast<uint32_t>(pl1);
    return (prio << kPrioPos) | (dst << kDstPos) | (src << kSrcPos) | (proto << kProtoPos)
        | (hi << kPl0Pos) | (lo << kPl1Pos);
}

inline void unpackId(uint32_t id, BaseHdr& h, uint8_t& pl0, uint8_t& pl1) noexcept
{
    h.prio = static_cast<uint8_t>((id >> kPrioPos) & kPrioMsk);
    h.dst = addr_t{static_cast<uint8_t>((id >> kDstPos) & kAddrMsk)};
    h.src = addr_t{static_cast<uint8_t>((id >> kSrcPos) & kAddrMsk)};
    h.proto = static_cast<uint8_t>((id >> kProtoPos) & kProtoMsk);
    pl0 = static_cast<uint8_t>((id >> kPl0Pos) & kPlHiMsk);
    pl1 = static_cast<uint8_t>(id >> kPl1Pos);
}

class CanStream : public BIF::IFrameStream<Frame> {
public:
    explicit CanStream(BIF::CAN::ICAN& can) noexcept
        : _can(can)
    {}

    [[nodiscard]] bool send(const Frame& frame) noexcept override
    {
        BIF::CAN::Frame wire{};
        if (!encode(frame, wire)) {
            return false;
        }
        return _can.send(wire);
    }

    [[nodiscard]] bool recv(Frame& frame) noexcept override
    {
        BIF::CAN::Frame wire{};
        if (!_can.recv(wire)) {
            return false;
        }
        return decode(wire, frame);
    }

    [[nodiscard]] static bool encode(const Frame& in, BIF::CAN::Frame& wire) noexcept
    {
        const std::size_t n = in.payload.cursor();
        if (n > kPayload) {
            return false;
        }
        const uint8_t* d = in.payload.data();
        const uint8_t pl0 = (n > 0u && d != nullptr) ? d[0] : 0u;
        const uint8_t pl1 = (n > 1u && d != nullptr) ? d[1] : 0u;
        const std::size_t tail = (n > kIdPl) ? (n - kIdPl) : 0u;

        wire = {};
        wire.id.setExtended(true);
        wire.id.set(packId(in.hdr, pl0, pl1));
        wire.dlc = static_cast<uint8_t>(tail);
        if (tail != 0u) {
            std::memcpy(wire.data, d + kIdPl, tail);
        }
        return true;
    }

    [[nodiscard]] static bool decode(const BIF::CAN::Frame& wire, Frame& out) noexcept
    {
        if (!wire.id.extended || wire.dlc > BIF::CAN::kMaxDataLength) {
            return false;
        }
        out = {};
        uint8_t prefix[kIdPl]{};
        unpackId(wire.id.get(), out.hdr, prefix[0], prefix[1]);
        uint8_t raw[kPayload]{};
        raw[0] = prefix[0];
        raw[1] = prefix[1];
        if (wire.dlc != 0u) {
            std::memcpy(raw + kIdPl, wire.data, wire.dlc);
        }
        (void)out.payload.push(raw, kIdPl + wire.dlc);
        return true;
    }

private:
    BIF::CAN::ICAN& _can;
};

} // namespace smcp2
