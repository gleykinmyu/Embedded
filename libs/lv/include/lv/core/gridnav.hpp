#pragma once

/**
 * @file gridnav.hpp
 * @brief C++ wrapper for LVGL grid navigation
 *
 * Enables arrow-key / D-pad navigation among children of a container.
 * Useful for TV remotes, game controllers, or keyboard-driven UIs.
 * Requires LV_USE_GRIDNAV=1.
 *
 * @code
 * auto grid = lv::Box::create(parent);
 * // ... add child widgets ...
 * lv::gridnav::add(grid, lv::gridnav::rollover);
 *
 * // Later, manually focus a child:
 * lv::gridnav::set_focused(grid, child);
 *
 * // Remove when no longer needed:
 * lv::gridnav::remove(grid);
 * @endcode
 */

#include <lvgl.h>
#include "object.hpp"

#if LV_USE_GRIDNAV

namespace lv::gridnav {

// ==================== Control Flags ====================

constexpr auto none                  = LV_GRIDNAV_CTRL_NONE;
constexpr auto rollover              = LV_GRIDNAV_CTRL_ROLLOVER;
constexpr auto scroll_first          = LV_GRIDNAV_CTRL_SCROLL_FIRST;
constexpr auto horizontal_move_only  = LV_GRIDNAV_CTRL_HORIZONTAL_MOVE_ONLY;
constexpr auto vertical_move_only    = LV_GRIDNAV_CTRL_VERTICAL_MOVE_ONLY;

// ==================== Functions ====================

/// Enable grid navigation on a container
inline void add(ObjectView obj, lv_gridnav_ctrl_t ctrl = LV_GRIDNAV_CTRL_NONE) noexcept {
    lv_gridnav_add(obj.get(), ctrl);
}

/// Disable grid navigation on a container
inline void remove(ObjectView obj) noexcept {
    lv_gridnav_remove(obj.get());
}

/// Manually focus a child within a gridnav container
inline void set_focused(ObjectView container, ObjectView child,
                        lv_anim_enable_t anim = LV_ANIM_OFF) noexcept {
    lv_gridnav_set_focused(container.get(), child.get(), anim);
}

} // namespace lv::gridnav

#endif // LV_USE_GRIDNAV
