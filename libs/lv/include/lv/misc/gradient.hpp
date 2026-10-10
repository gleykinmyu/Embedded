#pragma once

/**
 * @file gradient.hpp
 * @brief Fluent gradient descriptor wrapper
 *
 * Provides a C++ fluent API for LVGL gradients. Supports:
 * - Simple 2-color gradients (horizontal, vertical)
 * - Complex gradients (linear at angles, radial, conical)
 * - Multi-stop gradients (up to LV_GRADIENT_MAX_STOPS colors)
 *
 * @note Complex gradients require LV_USE_DRAW_SW_COMPLEX_GRADIENTS=1 in lv_conf.h
 * @note Multi-stop gradients (>2 colors) require increasing LV_GRADIENT_MAX_STOPS
 */

#include <lvgl.h>

namespace lv {

/// Gradient extend modes (what happens beyond gradient bounds)
namespace grad_extend {
    inline constexpr lv_grad_extend_t pad = LV_GRAD_EXTEND_PAD;         ///< Extend edge colors
    inline constexpr lv_grad_extend_t repeat = LV_GRAD_EXTEND_REPEAT;   ///< Repeat pattern: A→B, A→B
    inline constexpr lv_grad_extend_t reflect = LV_GRAD_EXTEND_REFLECT; ///< Mirror pattern: A→B, B→A
}

/**
 * @brief Fluent gradient descriptor wrapper
 *
 * @warning **IMPORTANT: Gradient descriptors must be `static` or have a lifetime
 * longer than the widgets using them!** LVGL stores a pointer to the gradient,
 * not a copy. If the gradient goes out of scope, you'll get undefined behavior.
 *
 * @par Why static?
 * LVGL is designed for embedded systems where copying large structures is expensive.
 * The gradient descriptor contains color stops and parameters that LVGL references
 * directly. This is a deliberate design choice in LVGL, not a limitation of this wrapper.
 *
 * @par Simple gradients (2 colors, horizontal/vertical)
 * For basic gradients, you can use the simpler style properties instead:
 * @code
 * box.bg_color(lv::rgb(0xFF0000))
 *    .bg_grad_color(lv::rgb(0x0000FF))
 *    .bg_grad_dir(LV_GRAD_DIR_HOR);  // or LV_GRAD_DIR_VER
 * @endcode
 *
 * @par Complex gradients (angles, radial, conical, multi-stop)
 * Use GradDsc for advanced gradients:
 * @code
 * // Diagonal gradient - MUST be static!
 * static auto grad = lv::GradDsc()
 *     .colors(lv::rgb(0xFF0000), lv::rgb(0x0000FF))
 *     .linear(lv_pct(0), lv_pct(0), lv_pct(100), lv_pct(100));
 *
 * box.bg_opa(LV_OPA_COVER)      // Required for complex gradients!
 *    .bg_grad(grad.get());
 * @endcode
 *
 * @par Multi-stop rainbow gradient
 * @code
 * static auto rainbow = lv::GradDsc()
 *     .stop(lv::rgb(0xFF0000), 0)    // Red at 0%
 *     .stop(lv::rgb(0x00FF00), 128)  // Green at 50%
 *     .stop(lv::rgb(0x0000FF), 255)  // Blue at 100%
 *     .linear(lv_pct(0), lv_pct(50), lv_pct(100), lv_pct(50));
 * @endcode
 *
 * @par Radial gradient
 * @code
 * static auto radial = lv::GradDsc()
 *     .colors(lv::rgb(0xFFFFFF), lv::rgb(0x000000))
 *     .radial(lv_pct(50), lv_pct(50), lv_pct(100), lv_pct(100));
 * @endcode
 */
class GradDsc {
    lv_grad_dsc_t m_dsc{};

public:
    /// Default constructor - no gradient
    GradDsc() noexcept = default;

    /// Get underlying C descriptor
    [[nodiscard]] lv_grad_dsc_t* get() noexcept { return &m_dsc; }
    [[nodiscard]] const lv_grad_dsc_t* get() const noexcept { return &m_dsc; }

    /// Implicit conversion to lv_grad_dsc_t& for C API compatibility
    operator lv_grad_dsc_t&() noexcept { return m_dsc; }
    operator const lv_grad_dsc_t&() const noexcept { return m_dsc; }

    // ==================== Gradient Types ====================

    /// Simple horizontal gradient (left to right)
    GradDsc& horizontal() noexcept {
        lv_grad_horizontal_init(&m_dsc);
        return *this;
    }

    /// Simple vertical gradient (top to bottom)
    GradDsc& vertical() noexcept {
        lv_grad_vertical_init(&m_dsc);
        return *this;
    }

    /// Linear gradient at any angle
    GradDsc& linear(int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y,
                    lv_grad_extend_t extend = LV_GRAD_EXTEND_PAD) noexcept {
        lv_grad_linear_init(&m_dsc, from_x, from_y, to_x, to_y, extend);
        return *this;
    }

    /// Radial gradient
    GradDsc& radial(int32_t center_x, int32_t center_y, int32_t to_x, int32_t to_y,
                    lv_grad_extend_t extend = LV_GRAD_EXTEND_PAD) noexcept {
        lv_grad_radial_init(&m_dsc, center_x, center_y, to_x, to_y, extend);
        return *this;
    }

    /// Set focal point for radial gradient
    GradDsc& focal(int32_t center_x, int32_t center_y, int32_t radius) noexcept {
        lv_grad_radial_set_focal(&m_dsc, center_x, center_y, radius);
        return *this;
    }

    /// Conical gradient
    GradDsc& conical(int32_t center_x, int32_t center_y,
                     int32_t start_angle, int32_t end_angle,
                     lv_grad_extend_t extend = LV_GRAD_EXTEND_PAD) noexcept {
        lv_grad_conical_init(&m_dsc, center_x, center_y, start_angle, end_angle, extend);
        return *this;
    }

    // ==================== Color Stops ====================

    /// Add a color stop (frac: 0-255 position)
    GradDsc& stop(lv_color_t color, uint8_t frac, lv_opa_t opa = LV_OPA_COVER) noexcept {
        if (m_dsc.stops_count < LV_GRADIENT_MAX_STOPS) {
            m_dsc.stops[m_dsc.stops_count].color = color;
            m_dsc.stops[m_dsc.stops_count].frac = frac;
            m_dsc.stops[m_dsc.stops_count].opa = opa;
            m_dsc.stops_count++;
        }
        return *this;
    }

    /// Set two-color gradient (convenience)
    GradDsc& colors(lv_color_t start, lv_color_t end) noexcept {
        m_dsc.stops_count = 0;
        stop(start, 0);
        stop(end, 255);
        return *this;
    }

    // ==================== Extend Mode ====================

    /// Set extend mode
    GradDsc& extend(lv_grad_extend_t mode) noexcept {
        m_dsc.extend = mode;
        return *this;
    }
};

} // namespace lv
