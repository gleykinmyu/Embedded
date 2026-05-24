/**
 * @file menu_catalog.hpp
 * @brief Доступ к subcode-меню по enum (индекс в таблицу).
 */

#pragma once

#include "catalog/command_ids.hpp"
#include "catalog/extended_j_menu.hpp"
#include "catalog/menu_entry.hpp"
#include "catalog/osd_menu.hpp"
#include "catalog/video_menu.hpp"

#include <cstddef>
#include <cstdint>

namespace ccam::catalog {

inline constexpr const MenuSubcodeEntry& getOsdMenu(OsdItem item)
{
    return kOsdMenu[static_cast<size_t>(item)];
}

inline constexpr const MenuSubcodeEntry& getVideoMenu(VideoMenuItem item)
{
    return kVideoMenu[static_cast<size_t>(item)];
}

inline constexpr const MenuSubcodeEntry& getExtendedJMenu(ExtendedJItem item)
{
    return kExtendedJMenu[static_cast<size_t>(item)];
}

inline constexpr uint8_t subcode(OsdItem item)
{
    return getOsdMenu(item).subcode;
}

inline constexpr uint8_t subcode(VideoMenuItem item)
{
    return getVideoMenu(item).subcode;
}

inline constexpr uint8_t subcode(ExtendedJItem item)
{
    return getExtendedJMenu(item).subcode;
}

inline constexpr const char* menuName(OsdItem item)
{
    return getOsdMenu(item).name;
}

inline constexpr const char* menuName(VideoMenuItem item)
{
    return getVideoMenu(item).name;
}

inline constexpr const char* menuName(ExtendedJItem item)
{
    return getExtendedJMenu(item).name;
}

} // namespace ccam::catalog
