/**
 * @file extended_j_menu.hpp
 * @brief Extended QSJ:nn / OSJ:nn — HE/UE (ConvertibleProtocol.pdf v3.05, стр. 24–27).
 */

#pragma once

#include "catalog/command_ids.hpp"
#include "catalog/menu_entry.hpp"

#include <cstddef>

namespace ccam::catalog {

inline constexpr MenuSubcodeEntry kExtendedJMenu[] = {
    {0x22, "3G SDI Out HDR Output Select"},
    {0x23, "MONI Out Format"},
    {0x24, "MONI Out HDR Output Select"},
    {0x25, "HDMI Out Format"},
    {0x26, "HDMI Out HDR Output Select"},
    {0x27, "Color Bar Tone"},
    {0x35, "Save Preset Name (Single)"},
    {0x3C, "Preset Name / Preset Thumbnail Counter"},
    {0x3D, "Zoom Scale"},
    {0x40, "Operation Lock Status"},
    {0x45, "Power On Position"},
    {0x46, "Power On Preset Number"},
};

inline constexpr size_t kExtendedJMenuCount =
    sizeof(kExtendedJMenu) / sizeof(kExtendedJMenu[0]);

static_assert(
    kExtendedJMenuCount == static_cast<size_t>(ExtendedJItem::Count),
    "kExtendedJMenu vs ExtendedJItem");

} // namespace ccam::catalog
