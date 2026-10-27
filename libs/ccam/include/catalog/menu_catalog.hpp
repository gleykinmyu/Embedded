/**
 * @file menu_catalog.hpp
 * @brief Доступ к subcode-меню по enum (индекс в таблицу).
 */

#pragma once

#include "catalog/command_ids.hpp"
#include "catalog/extended_j_menu.hpp"
#include "catalog/osd_menu.hpp"
#include "catalog/video_menu.hpp"

#include <cstddef>
#include <cstdint>

namespace ccam::catalog {

inline constexpr uint8_t subcode(OsdItem item)
{
    return kOsdMenu[static_cast<size_t>(item)];
}

inline constexpr uint8_t subcode(VideoMenuItem item)
{
    return kVideoMenu[static_cast<size_t>(item)];
}

inline constexpr uint8_t subcode(ExtendedJItem item)
{
    return kExtendedJMenu[static_cast<size_t>(item)];
}

} // namespace ccam::catalog
