/**
 * @file fw_version.hpp
 * @brief MISC::FwVersion — версия прошивки одним словом uint32_t.
 *
 * Слово: major [31:24] | minor [23:16] | patch [15:0].
 * На проводе — это слово, младший байт первый (pushU32 / load uint32_t):
 * patch_lo, patch_hi, minor, major.
 *
 * major — прошивки вместе не работают.
 * minor — расширение внутри того же major.
 * patch — на совместимость не влияет.
 *
 * Номер этой прошивки — kFw, из -DFW_VERSION_MAJOR/MINOR/PATCH.
 * Пока флагов нет — 0.0.0.
 */

#pragma once

#include <cstdint>

namespace MISC {

struct FwVersion {
    uint8_t major = 0;
    uint8_t minor = 0;
    uint16_t patch = 0;

    [[nodiscard]] constexpr uint32_t pack() const noexcept
    {
        return (static_cast<uint32_t>(major) << 24)
            | (static_cast<uint32_t>(minor) << 16)
            | static_cast<uint32_t>(patch);
    }

    [[nodiscard]] static constexpr FwVersion unpack(uint32_t w) noexcept
    {
        return FwVersion{
            static_cast<uint8_t>(w >> 24),
            static_cast<uint8_t>(w >> 16),
            static_cast<uint16_t>(w),
        };
    }
};

static_assert(sizeof(FwVersion) == 4u, "FwVersion: 4 байта");

enum class FwFit : uint8_t {
    Ok,    ///< тот же major, minor соседа не новее нашего
    Ahead, ///< тот же major, minor соседа больше
    Other, ///< другой major
};

/** Ok — тот же major и minor соседа не новее. Ahead — сосед новее по minor. Other — другой major. */
[[nodiscard]] constexpr FwFit fwFit(FwVersion self, FwVersion peer) noexcept
{
    if (self.major != peer.major) {
        return FwFit::Other;
    }
    if (peer.minor > self.minor) {
        return FwFit::Ahead;
    }
    return FwFit::Ok;
}

#ifndef FW_VERSION_MAJOR
#define FW_VERSION_MAJOR 0
#endif
#ifndef FW_VERSION_MINOR
#define FW_VERSION_MINOR 0
#endif
#ifndef FW_VERSION_PATCH
#define FW_VERSION_PATCH 0
#endif

inline constexpr FwVersion kFw{FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH};

static_assert(FwVersion{1, 2, 3}.pack() == 0x01020003u);
static_assert(FwVersion::unpack(0x01020003u).major == 1u);
static_assert(FwVersion::unpack(0x01020003u).minor == 2u);
static_assert(FwVersion::unpack(0x01020003u).patch == 3u);
static_assert(fwFit(FwVersion{1, 2, 0}, FwVersion{1, 2, 9}) == FwFit::Ok);
static_assert(fwFit(FwVersion{1, 2, 0}, FwVersion{1, 1, 0}) == FwFit::Ok);
static_assert(fwFit(FwVersion{1, 2, 0}, FwVersion{1, 3, 0}) == FwFit::Ahead);
static_assert(fwFit(FwVersion{1, 0, 0}, FwVersion{2, 0, 0}) == FwFit::Other);

} // namespace MISC
