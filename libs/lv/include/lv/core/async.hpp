#pragma once

/**
 * @file async.hpp
 * @brief Zero-cost wrapper for LVGL deferred calls
 *
 * Schedule a function to run on the next LVGL tick.
 * Useful for deferring UI updates from event callbacks.
 *
 * Usage:
 *   lv::async_call([](void*) { ... });
 */

#include <lvgl.h>
#include <type_traits>

namespace lv {

/// Schedule a deferred call on the next LVGL tick
[[nodiscard]] inline bool async_call(lv_async_cb_t cb, void* user_data = nullptr) noexcept {
    return lv_async_call(cb, user_data) == LV_RESULT_OK;
}

/// Cancel a pending deferred call
[[nodiscard]] inline bool async_call_cancel(lv_async_cb_t cb, void* user_data = nullptr) noexcept {
    return lv_async_call_cancel(cb, user_data) == LV_RESULT_OK;
}

/// Schedule a deferred member function call (zero-cost trampoline)
template<auto MemFn, typename T>
    requires std::is_member_function_pointer_v<decltype(MemFn)>
[[nodiscard]] bool async_call(T* instance) noexcept {
    return lv_async_call([](void* data) {
        (static_cast<T*>(data)->*MemFn)();
    }, instance) == LV_RESULT_OK;
}

} // namespace lv
