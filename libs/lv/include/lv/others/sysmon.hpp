#pragma once

/**
 * @file sysmon.hpp
 * @brief Zero-cost wrapper for LVGL system monitor
 *
 * On-screen display of CPU usage, FPS, and memory stats.
 *
 * Usage:
 *   lv::sysmon::show_performance();   // show CPU/FPS overlay
 *   lv::sysmon::show_memory();        // show memory overlay
 */

#include <lvgl.h>
#include "../core/object.hpp"

#if LV_USE_SYSMON

namespace lv::sysmon {

/// Create a system monitor label on the display's system layer
inline ObjectView create(lv_display_t* disp) noexcept {
    return ObjectView(lv_sysmon_create(disp));
}

#if LV_USE_PERF_MONITOR

/// Show CPU usage and FPS (nullptr = default display)
inline void show_performance(lv_display_t* disp = nullptr) noexcept {
    lv_sysmon_show_performance(disp);
}

/// Hide performance monitor
inline void hide_performance(lv_display_t* disp = nullptr) noexcept {
    lv_sysmon_hide_performance(disp);
}

/// Pause performance monitoring
inline void pause_performance(lv_display_t* disp = nullptr) noexcept {
    lv_sysmon_performance_pause(disp);
}

/// Resume performance monitoring
inline void resume_performance(lv_display_t* disp = nullptr) noexcept {
    lv_sysmon_performance_resume(disp);
}

/// Dump FPS data recorded since last dump
inline void dump_performance(lv_display_t* disp = nullptr) noexcept {
    lv_sysmon_performance_dump(disp);
}

#endif // LV_USE_PERF_MONITOR

#if LV_USE_MEM_MONITOR

/// Show memory usage and fragmentation (nullptr = default display)
inline void show_memory(lv_display_t* disp = nullptr) noexcept {
    lv_sysmon_show_memory(disp);
}

/// Hide memory monitor
inline void hide_memory(lv_display_t* disp = nullptr) noexcept {
    lv_sysmon_hide_memory(disp);
}

#endif // LV_USE_MEM_MONITOR

} // namespace lv::sysmon

#endif // LV_USE_SYSMON
