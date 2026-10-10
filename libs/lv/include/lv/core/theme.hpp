#pragma once

/**
 * @file theme.hpp
 * @brief Zero-cost theme wrapper for LVGL
 *
 * Provides C++ API for LVGL's theming system.
 */

#include <lvgl.h>
#include "object.hpp"
#include "version.hpp"

namespace lv {

/**
 * @brief Theme wrapper
 *
 * Wraps LVGL's theme system for styling widgets.
 */
class Theme {
    lv_theme_t* m_theme;

public:
    /// Default constructor (null theme)
    Theme() noexcept : m_theme(nullptr) {}

    /// Wrap existing theme (non-owning)
    explicit Theme(lv_theme_t* theme) noexcept : m_theme(theme) {}

    ~Theme() = default;

    // Copyable (non-owning wrapper)
    Theme(const Theme&) = default;
    Theme& operator=(const Theme&) = default;

    // Moveable
    Theme(Theme&& other) noexcept : m_theme(other.m_theme) {
        other.m_theme = nullptr;
    }

    Theme& operator=(Theme&& other) noexcept {
        if (this != &other) {
            m_theme = other.m_theme;
            other.m_theme = nullptr;
        }
        return *this;
    }

    /// Get underlying theme pointer
    [[nodiscard]] lv_theme_t* get() noexcept { return m_theme; }
    [[nodiscard]] const lv_theme_t* get() const noexcept { return m_theme; }

    /// Set parent theme (for theme inheritance)
    Theme& parent(lv_theme_t* parent_theme) noexcept {
        lv_theme_set_parent(m_theme, parent_theme);
        return *this;
    }

    Theme& parent(Theme& parent_theme) noexcept {
        return parent(parent_theme.get());
    }

    /// Set apply callback
    Theme& apply_cb(lv_theme_apply_cb_t cb) noexcept {
        lv_theme_set_apply_cb(m_theme, cb);
        return *this;
    }
};

// ==================== Theme Helpers ====================

/// Get theme from object's display
inline lv_theme_t* theme_get(ObjectView obj) noexcept {
    return lv_theme_get_from_obj(obj.get());
}

/// Apply theme to object
inline void theme_apply(ObjectView obj) noexcept {
    lv_theme_apply(obj.get());
}

/// Get small font from theme
inline const lv_font_t* theme_font_small(ObjectView obj) noexcept {
    return lv_theme_get_font_small(obj.get());
}

/// Get normal font from theme
inline const lv_font_t* theme_font_normal(ObjectView obj) noexcept {
    return lv_theme_get_font_normal(obj.get());
}

/// Get large font from theme
inline const lv_font_t* theme_font_large(ObjectView obj) noexcept {
    return lv_theme_get_font_large(obj.get());
}

/// Get primary color from theme
inline lv_color_t theme_color_primary(ObjectView obj) noexcept {
    return lv_theme_get_color_primary(obj.get());
}

/// Get secondary color from theme
inline lv_color_t theme_color_secondary(ObjectView obj) noexcept {
    return lv_theme_get_color_secondary(obj.get());
}

// ==================== Default Theme ====================

#if LV_USE_THEME_DEFAULT

/// Initialize default theme
inline lv_theme_t* theme_default_init(lv_display_t* disp, lv_color_t primary, lv_color_t secondary,
                                       bool dark, const lv_font_t* font) noexcept {
    return lv_theme_default_init(disp, primary, secondary, dark, font);
}

/// Get default theme
inline lv_theme_t* theme_default_get() noexcept {
    return lv_theme_default_get();
}

/// Check if default theme is inited
inline bool theme_default_is_inited() noexcept {
    return lv_theme_default_is_inited();
}

/// Deinitialize default theme
inline void theme_default_deinit() noexcept {
    lv_theme_default_deinit();
}

#endif // LV_USE_THEME_DEFAULT

// ==================== Simple Theme ====================

#if LV_USE_THEME_SIMPLE

/// Initialize simple theme
inline lv_theme_t* theme_simple_init(lv_display_t* disp) noexcept {
    return lv_theme_simple_init(disp);
}

/// Get simple theme
inline lv_theme_t* theme_simple_get() noexcept {
    return lv_theme_simple_get();
}

/// Check if simple theme is inited
inline bool theme_simple_is_inited() noexcept {
    return lv_theme_simple_is_inited();
}

/// Deinitialize simple theme
inline void theme_simple_deinit() noexcept {
    lv_theme_simple_deinit();
}

#endif // LV_USE_THEME_SIMPLE

// ==================== Mono Theme ====================

#if LV_USE_THEME_MONO

/// Initialize mono (monochrome) theme
inline lv_theme_t* theme_mono_init(lv_display_t* disp, bool dark_bg, const lv_font_t* font) noexcept {
    return lv_theme_mono_init(disp, dark_bg, font);
}

/// Get mono theme
inline lv_theme_t* theme_mono_get() noexcept {
    return lv_theme_mono_get();
}

/// Check if mono theme is inited
inline bool theme_mono_is_inited() noexcept {
    return lv_theme_mono_is_inited();
}

/// Deinitialize mono theme
inline void theme_mono_deinit() noexcept {
    lv_theme_mono_deinit();
}

#endif // LV_USE_THEME_MONO

// ==================== Theme Lifecycle (LVGL 9.5+) ====================

#if LV_VERSION_AT_LEAST(9, 5, 0)

/// Create a new empty theme (LVGL 9.5+)
inline Theme theme_create() noexcept {
    return Theme(lv_theme_create());
}

/// Copy theme properties from src to dst (LVGL 9.5+)
inline void theme_copy(Theme& dst, const Theme& src) noexcept {
    lv_theme_copy(dst.get(), src.get());
}

/// Delete a dynamically created theme and reset the wrapper (LVGL 9.5+)
inline void theme_delete(Theme& theme) noexcept {
    lv_theme_delete(theme.get());
    theme = Theme();
}

#endif // LV_VERSION_AT_LEAST(9, 5, 0)

} // namespace lv
