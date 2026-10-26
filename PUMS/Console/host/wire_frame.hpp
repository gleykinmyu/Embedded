/**
 * @file wire_frame.hpp
 * @brief Сериализация BIF::CAN::Frame для TCP bus (13 байт LE).
 */

#pragma once

#include <cstdint>
#include <cstring>

#include "ican.hpp"

namespace smcp {
namespace host {

inline constexpr std::size_t kWireFrameSize = 13u;

inline bool packWireFrame(const BIF::CAN::Frame& frame, uint8_t out[kWireFrameSize]) noexcept
{
    const uint32_t packed = frame.id.pack();
    out[0] = static_cast<uint8_t>(packed);
    out[1] = static_cast<uint8_t>(packed >> 8);
    out[2] = static_cast<uint8_t>(packed >> 16);
    out[3] = static_cast<uint8_t>(packed >> 24);
    out[4] = frame.dlc > 8u ? 8u : frame.dlc;
    std::memcpy(out + 5, frame.data, 8);
    return true;
}

inline bool unpackWireFrame(const uint8_t in[kWireFrameSize], BIF::CAN::Frame& out) noexcept
{
    const uint32_t packed = static_cast<uint32_t>(in[0])
        | (static_cast<uint32_t>(in[1]) << 8)
        | (static_cast<uint32_t>(in[2]) << 16)
        | (static_cast<uint32_t>(in[3]) << 24);
    out.id = BIF::CAN::Frame::Id::unpack(packed);
    out.dlc = in[4] > 8u ? 8u : in[4];
    std::memcpy(out.data, in + 5, 8);
    return true;
}

} // namespace host
} // namespace smcp
