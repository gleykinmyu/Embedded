#pragma once

/**
 * @file snapshot.hpp
 * @brief C++ wrapper for LVGL snapshot API
 *
 * Captures an object (with its children) to an image buffer.
 * Requires LV_USE_SNAPSHOT=1.
 *
 * @code
 * auto buf = lv::snapshot::take(my_widget);
 * if (buf) {
 *     lv::Image::create(parent).src(buf);  // display the snapshot
 *     // ...
 *     lv_draw_buf_destroy(buf);            // free when done
 * }
 *
 * // Or reuse a draw buffer across frames:
 * lv_draw_buf_t* buf = lv::snapshot::create_draw_buf(widget);
 * lv::snapshot::take_to(widget, buf);
 * @endcode
 */

#include <lvgl.h>
#include "object.hpp"

#if LV_USE_SNAPSHOT

namespace lv::snapshot {

/// Take a snapshot, allocating a new draw buffer. Caller must lv_draw_buf_destroy() it.
[[nodiscard]] inline lv_draw_buf_t* take(ObjectView obj,
                           lv_color_format_t cf = LV_COLOR_FORMAT_ARGB8888) noexcept {
    return lv_snapshot_take(obj.get(), cf);
}

/// Create a draw buffer sized for the given object (for reuse with take_to)
[[nodiscard]] inline lv_draw_buf_t* create_draw_buf(ObjectView obj,
                                      lv_color_format_t cf = LV_COLOR_FORMAT_ARGB8888) noexcept {
    return lv_snapshot_create_draw_buf(obj.get(), cf);
}

/// Reshape an existing draw buffer to fit the object (returns false if buffer must be recreated)
[[nodiscard]] inline bool reshape_draw_buf(ObjectView obj, lv_draw_buf_t* buf) noexcept {
    return lv_snapshot_reshape_draw_buf(obj.get(), buf) == LV_RESULT_OK;
}

/// Take a snapshot into an existing draw buffer (reshapes automatically)
[[nodiscard]] inline bool take_to(ObjectView obj, lv_draw_buf_t* buf,
                    lv_color_format_t cf = LV_COLOR_FORMAT_ARGB8888) noexcept {
    return lv_snapshot_take_to_draw_buf(obj.get(), cf, buf) == LV_RESULT_OK;
}

} // namespace lv::snapshot

#endif // LV_USE_SNAPSHOT
