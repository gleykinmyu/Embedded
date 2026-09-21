/**
 * @file netutil.hpp
 * @brief Big/little endian и multicast sACN.
 */
#pragma once

#include "iudp.hpp"

#include <cstdint>

namespace dmx {
namespace net {

inline void putBe16(uint8_t* p, uint16_t v) noexcept
{
    p[0] = static_cast<uint8_t>(v >> 8);
    p[1] = static_cast<uint8_t>(v);
}

inline void putBe32(uint8_t* p, uint32_t v) noexcept
{
    p[0] = static_cast<uint8_t>(v >> 24);
    p[1] = static_cast<uint8_t>(v >> 16);
    p[2] = static_cast<uint8_t>(v >> 8);
    p[3] = static_cast<uint8_t>(v);
}

[[nodiscard]] inline uint16_t be16(const uint8_t* p) noexcept
{
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

[[nodiscard]] inline uint32_t be32(const uint8_t* p) noexcept
{
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16)
        | (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

inline void putLe16(uint8_t* p, uint16_t v) noexcept
{
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
}

[[nodiscard]] inline uint16_t le16(const uint8_t* p) noexcept
{
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

/// Art-Net 15-bit: net (7) | subnet (4) | universe (4).
[[nodiscard]] constexpr uint16_t packArtNet(uint8_t net, uint8_t subnet, uint8_t uni) noexcept
{
    return static_cast<uint16_t>((static_cast<uint16_t>(net & 0x7Fu) << 8)
        | (static_cast<uint16_t>(subnet & 0x0Fu) << 4) | (uni & 0x0Fu));
}

[[nodiscard]] constexpr uint8_t artNet(uint16_t packed) noexcept
{
    return static_cast<uint8_t>((packed >> 8) & 0x7Fu);
}

[[nodiscard]] constexpr uint8_t artSubnet(uint16_t packed) noexcept
{
    return static_cast<uint8_t>((packed >> 4) & 0x0Fu);
}

[[nodiscard]] constexpr uint8_t artUniverse(uint16_t packed) noexcept
{
    return static_cast<uint8_t>(packed & 0x0Fu);
}

/// E1.31 multicast: 239.255.(universe>>8).(universe&0xFF), universe 1…63999.
[[nodiscard]] constexpr BIF::NET::Ipv4 sacnGroup(uint16_t universe) noexcept
{
    return BIF::NET::ipv4(239, 255, static_cast<uint8_t>(universe >> 8),
        static_cast<uint8_t>(universe));
}

} // namespace net
} // namespace dmx
