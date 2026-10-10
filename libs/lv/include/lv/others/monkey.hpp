#pragma once

/**
 * @file monkey.hpp
 * @brief Zero-cost wrapper for LVGL monkey testing
 *
 * Automated UI testing by generating random input events.
 *
 * Usage:
 *   lv::Monkey monkey(LV_INDEV_TYPE_POINTER, 100, 500);
 *   monkey.enable();
 */

#include <lvgl.h>

#if LV_USE_MONKEY

namespace lv {

/**
 * @brief RAII wrapper for the LVGL monkey tester
 *
 * Generates random input events for UI stress testing.
 *
 * Size: sizeof(void*) — pointer-sized
 */
class Monkey {
    lv_monkey_t* m_monkey = nullptr;

public:
    /// Create a monkey with input type and period range (ms)
    Monkey(lv_indev_type_t type, uint32_t period_min, uint32_t period_max) noexcept {
        lv_monkey_config_t config;
        lv_monkey_config_init(&config);
        config.type = type;
        config.period_range.min = period_min;
        config.period_range.max = period_max;
        m_monkey = lv_monkey_create(&config);
    }

    /// Create with full config
    explicit Monkey(const lv_monkey_config_t& config) noexcept
        : m_monkey(lv_monkey_create(&config)) {}

    ~Monkey() noexcept {
        if (m_monkey) lv_monkey_delete(m_monkey);
    }

    // Move-only
    Monkey(const Monkey&) = delete;
    Monkey& operator=(const Monkey&) = delete;

    Monkey(Monkey&& other) noexcept : m_monkey(other.m_monkey) {
        other.m_monkey = nullptr;
    }

    Monkey& operator=(Monkey&& other) noexcept {
        if (this != &other) {
            if (m_monkey) lv_monkey_delete(m_monkey);
            m_monkey = other.m_monkey;
            other.m_monkey = nullptr;
        }
        return *this;
    }

    // ==================== Control ====================

    /// Enable/disable the monkey
    Monkey& enable(bool en = true) noexcept {
        lv_monkey_set_enable(m_monkey, en);
        return *this;
    }

    /// Disable the monkey
    Monkey& disable() noexcept {
        return enable(false);
    }

    /// Check if the monkey is enabled
    [[nodiscard]] bool is_enabled() const noexcept {
        return lv_monkey_get_enable(m_monkey);
    }

    // ==================== User Data ====================

    /// Set user data
    Monkey& user_data(void* data) noexcept {
        lv_monkey_set_user_data(m_monkey, data);
        return *this;
    }

    /// Get user data
    [[nodiscard]] void* user_data() const noexcept {
        return lv_monkey_get_user_data(m_monkey);
    }

    // ==================== Access ====================

    /// Get the associated input device
    [[nodiscard]] lv_indev_t* indev() noexcept {
        return lv_monkey_get_indev(m_monkey);
    }

    /// Get underlying monkey pointer
    [[nodiscard]] lv_monkey_t* get() noexcept { return m_monkey; }
    [[nodiscard]] const lv_monkey_t* get() const noexcept { return m_monkey; }

    /// Check if valid
    [[nodiscard]] explicit operator bool() const noexcept { return m_monkey != nullptr; }
};

} // namespace lv

#endif // LV_USE_MONKEY
