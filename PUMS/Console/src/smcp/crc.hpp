/**
 * @file crc.hpp
 * @brief CRC16-CCITT и CRC32 (ISO-HDLC) для шоуфайла / SMCP.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace smcp {

/** CRC16-CCITT: poly 0x1021, init 0xFFFF, xorout 0x0000, refin/refout false. */
[[nodiscard]] inline uint16_t crc16Ccitt(const uint8_t* data, std::size_t len,
                                         uint16_t crc = 0xFFFFu) noexcept
{
    if (data == nullptr && len != 0u) {
        return crc;
    }
    for (std::size_t i = 0; i < len; ++i) {
        crc = static_cast<uint16_t>(crc ^ (static_cast<uint16_t>(data[i]) << 8));
        for (uint8_t b = 0; b < 8u; ++b) {
            if ((crc & 0x8000u) != 0u) {
                crc = static_cast<uint16_t>((crc << 1) ^ 0x1021u);
            } else {
                crc = static_cast<uint16_t>(crc << 1);
            }
        }
    }
    return crc;
}

/** CRC32 ISO-HDLC: poly 0xEDB88320, init/xorout 0xFFFFFFFF. */
[[nodiscard]] inline uint32_t crc32Update(uint32_t crc, const uint8_t* data, std::size_t len) noexcept
{
    if (data == nullptr && len != 0u) {
        return crc;
    }
    for (std::size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8u; ++b) {
            const uint32_t mask = static_cast<uint32_t>(-(static_cast<int32_t>(crc & 1u)));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc;
}

[[nodiscard]] inline uint32_t crc32(const uint8_t* data, std::size_t len) noexcept
{
    return crc32Update(0xFFFFFFFFu, data, len) ^ 0xFFFFFFFFu;
}

[[nodiscard]] inline constexpr uint32_t crc32Init() noexcept { return 0xFFFFFFFFu; }

[[nodiscard]] inline constexpr uint32_t crc32Final(uint32_t crc) noexcept { return crc ^ 0xFFFFFFFFu; }

} // namespace smcp
