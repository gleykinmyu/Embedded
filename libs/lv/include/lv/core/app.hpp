#pragma once

/**
 * @file app.hpp
 * @brief Application lifecycle management
 *
 * Provides RAII initialization and main loop helpers.
 */

#include <lvgl.h>
#include <cstdint>

#ifdef __unix__
#include <unistd.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

namespace lv {

/**
 * @brief Initialize LVGL
 *
 * Must be called before creating any displays or widgets.
 */
inline void init() noexcept {
    lv_init();
}

/**
 * @brief Deinitialize LVGL
 *
 * Call when shutting down the application.
 */
inline void deinit() noexcept {
    lv_deinit();
}

/**
 * @brief Sleep for milliseconds (platform-independent)
 */
inline void sleep_ms(uint32_t ms) noexcept {
#ifdef __unix__
    usleep(ms * 1000);
#elif defined(_WIN32)
    Sleep(ms);
#else
    // Fallback: busy wait (not recommended)
    volatile uint32_t count = ms * 1000;
    while (count--) {}
#endif
}

/**
 * @brief Run one iteration of the main loop
 *
 * Processes LVGL timers and returns time until next call needed.
 *
 * @return Milliseconds until next call needed
 */
inline uint32_t tick() noexcept {
    return lv_timer_handler();
}

/**
 * @brief Run the main loop forever
 *
 * This function never returns. Call after setting up UI.
 *
 * Usage:
 * @code
 * lv::init();
 * auto display = lv::X11Display("My App", 800, 480);
 * MyComponent app;
 * app.mount(lv::screen_active());
 * lv::run();  // Never returns
 * @endcode
 */
[[noreturn]] inline void run() noexcept {
    while (true) {
        uint32_t time_till_next = lv_timer_handler();
        sleep_ms(time_till_next);
    }
}

/**
 * @brief Run the main loop with a custom tick callback
 *
 * Allows injecting custom logic into the main loop.
 *
 * @param on_tick Called every iteration. Return false to exit.
 *
 * Usage:
 * @code
 * bool running = true;
 * lv::run_with([&]() {
 *     // Custom logic here
 *     return running;  // Return false to exit
 * });
 * @endcode
 */
template<typename F>
inline void run_with(F&& on_tick) {
    while (on_tick()) {
        uint32_t time_till_next = lv_timer_handler();
        sleep_ms(time_till_next);
    }
}

/**
 * @brief Run for a limited number of iterations (for testing)
 *
 * @param max_iterations Maximum number of loop iterations
 */
inline void run_for(uint32_t max_iterations) noexcept {
    for (uint32_t i = 0; i < max_iterations; ++i) {
        uint32_t time_till_next = lv_timer_handler();
        sleep_ms(time_till_next);
    }
}


/**
 * @brief RAII guard for LVGL initialization
 *
 * Automatically calls lv_init() on construction and lv_deinit() on destruction.
 *
 * Usage:
 * @code
 * int main() {
 *     lv::InitGuard guard;  // Calls lv_init()
 *     // ... create display and UI ...
 *     lv::run();
 * }  // guard destructor calls lv_deinit()
 * @endcode
 */
class InitGuard {
public:
    InitGuard() noexcept { lv_init(); }
    ~InitGuard() { lv_deinit(); }

    // Non-copyable, non-movable
    InitGuard(const InitGuard&) = delete;
    InitGuard& operator=(const InitGuard&) = delete;
    InitGuard(InitGuard&&) = delete;
    InitGuard& operator=(InitGuard&&) = delete;
};

} // namespace lv
