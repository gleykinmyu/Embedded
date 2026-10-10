#pragma once

/**
 * @file log.hpp
 * @brief Zero-cost wrapper for LVGL logging
 *
 * Provides log callback registration and convenience log functions.
 * Uses std::source_location to capture caller file/line/function.
 *
 * Usage:
 *   lv::log::set_print_cb([](lv_log_level_t level, const char* msg) {
 *       printf("[%d] %s\n", level, msg);
 *   });
 *   lv::log::info("value = %d", 42);
 *   lv::log::warn("something happened");
 */

#include <lvgl.h>
#include <source_location>

#if LV_USE_LOG

namespace lv::log {

/// Log level constants
namespace level {
    constexpr lv_log_level_t trace = LV_LOG_LEVEL_TRACE;
    constexpr lv_log_level_t info  = LV_LOG_LEVEL_INFO;
    constexpr lv_log_level_t warn  = LV_LOG_LEVEL_WARN;
    constexpr lv_log_level_t error = LV_LOG_LEVEL_ERROR;
    constexpr lv_log_level_t user  = LV_LOG_LEVEL_USER;
    constexpr lv_log_level_t none  = LV_LOG_LEVEL_NONE;
}

/// Register a custom log print callback
inline void set_print_cb(lv_log_print_g_cb_t cb) noexcept {
    lv_log_register_print_cb(cb);
}

/// Format string with source location captured at the call site.
/// Implicitly converts from const char*, so callers just write:
///   lv::log::info("x = %d", 42);
struct Fmt {
    const char* str;
    std::source_location loc;

    Fmt(const char* s, const std::source_location& l = std::source_location::current()) noexcept
        : str(s), loc(l) {}
};

namespace detail {

/// Dispatch log call. Uses "%s" for no-arg calls to avoid -Wformat-security.
template<typename... Args>
inline void log_add(lv_log_level_t lvl, const Fmt& fmt, Args... args) noexcept {
    if constexpr (sizeof...(Args) == 0) {
        lv_log_add(lvl, fmt.loc.file_name(), static_cast<int>(fmt.loc.line()),
                   fmt.loc.function_name(), "%s", fmt.str);
    } else {
        lv_log_add(lvl, fmt.loc.file_name(), static_cast<int>(fmt.loc.line()),
                   fmt.loc.function_name(), fmt.str, args...);
    }
}

} // namespace detail

/// Log at trace level
template<typename... Args>
inline void trace(Fmt fmt, Args... args) noexcept {
    detail::log_add(LV_LOG_LEVEL_TRACE, fmt, args...);
}

/// Log at info level
template<typename... Args>
inline void info(Fmt fmt, Args... args) noexcept {
    detail::log_add(LV_LOG_LEVEL_INFO, fmt, args...);
}

/// Log at warn level
template<typename... Args>
inline void warn(Fmt fmt, Args... args) noexcept {
    detail::log_add(LV_LOG_LEVEL_WARN, fmt, args...);
}

/// Log at error level
template<typename... Args>
inline void error(Fmt fmt, Args... args) noexcept {
    detail::log_add(LV_LOG_LEVEL_ERROR, fmt, args...);
}

/// Log at user level
template<typename... Args>
inline void user(Fmt fmt, Args... args) noexcept {
    detail::log_add(LV_LOG_LEVEL_USER, fmt, args...);
}

} // namespace lv::log

#else // !LV_USE_LOG

namespace lv::log {

template<typename... Args>
inline void trace(const char*, Args...) noexcept {}
template<typename... Args>
inline void info(const char*, Args...) noexcept {}
template<typename... Args>
inline void warn(const char*, Args...) noexcept {}
template<typename... Args>
inline void error(const char*, Args...) noexcept {}
template<typename... Args>
inline void user(const char*, Args...) noexcept {}

} // namespace lv::log

#endif // LV_USE_LOG
