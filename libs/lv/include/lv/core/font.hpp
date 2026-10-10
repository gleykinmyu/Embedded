#pragma once

/**
 * @file font.hpp
 * @brief Font references for LVGL fonts
 *
 * Provides clean C++ references to LVGL built-in fonts.
 */

#include <lvgl.h>

namespace lv {

/**
 * @brief Font reference wrapper
 *
 * Non-owning reference to an LVGL font.
 * Zero overhead - just a pointer.
 */
class Font {
    const lv_font_t* m_font;

public:
    constexpr Font(const lv_font_t* f) noexcept : m_font(f) {}
    constexpr Font(const lv_font_t& f) noexcept : m_font(&f) {}

    [[nodiscard]] constexpr const lv_font_t* get() const noexcept { return m_font; }
    [[nodiscard]] constexpr operator const lv_font_t*() const noexcept { return m_font; }

    [[nodiscard]] int32_t line_height() const noexcept {
        return lv_font_get_line_height(m_font);
    }
};

/**
 * @namespace fonts
 * @brief Built-in LVGL Montserrat fonts
 *
 * Usage:
 * @code
 * lv::Label::create(parent)
 *     .text("Hello")
 *     .font(lv::fonts::montserrat_20);
 * @endcode
 */
namespace fonts {

#if LV_FONT_MONTSERRAT_8
inline const Font montserrat_8{lv_font_montserrat_8};
#endif

#if LV_FONT_MONTSERRAT_10
inline const Font montserrat_10{lv_font_montserrat_10};
#endif

#if LV_FONT_MONTSERRAT_12
inline const Font montserrat_12{lv_font_montserrat_12};
#endif

#if LV_FONT_MONTSERRAT_14
inline const Font montserrat_14{lv_font_montserrat_14};
#endif

#if LV_FONT_MONTSERRAT_16
inline const Font montserrat_16{lv_font_montserrat_16};
#endif

#if LV_FONT_MONTSERRAT_18
inline const Font montserrat_18{lv_font_montserrat_18};
#endif

#if LV_FONT_MONTSERRAT_20
inline const Font montserrat_20{lv_font_montserrat_20};
#endif

#if LV_FONT_MONTSERRAT_22
inline const Font montserrat_22{lv_font_montserrat_22};
#endif

#if LV_FONT_MONTSERRAT_24
inline const Font montserrat_24{lv_font_montserrat_24};
#endif

#if LV_FONT_MONTSERRAT_26
inline const Font montserrat_26{lv_font_montserrat_26};
#endif

#if LV_FONT_MONTSERRAT_28
inline const Font montserrat_28{lv_font_montserrat_28};
#endif

#if LV_FONT_MONTSERRAT_30
inline const Font montserrat_30{lv_font_montserrat_30};
#endif

#if LV_FONT_MONTSERRAT_32
inline const Font montserrat_32{lv_font_montserrat_32};
#endif

#if LV_FONT_MONTSERRAT_34
inline const Font montserrat_34{lv_font_montserrat_34};
#endif

#if LV_FONT_MONTSERRAT_36
inline const Font montserrat_36{lv_font_montserrat_36};
#endif

#if LV_FONT_MONTSERRAT_38
inline const Font montserrat_38{lv_font_montserrat_38};
#endif

#if LV_FONT_MONTSERRAT_40
inline const Font montserrat_40{lv_font_montserrat_40};
#endif

#if LV_FONT_MONTSERRAT_42
inline const Font montserrat_42{lv_font_montserrat_42};
#endif

#if LV_FONT_MONTSERRAT_44
inline const Font montserrat_44{lv_font_montserrat_44};
#endif

#if LV_FONT_MONTSERRAT_46
inline const Font montserrat_46{lv_font_montserrat_46};
#endif

#if LV_FONT_MONTSERRAT_48
inline const Font montserrat_48{lv_font_montserrat_48};
#endif

/// Default font (typically montserrat_14)
inline const Font default_font{LV_FONT_DEFAULT};

} // namespace fonts

} // namespace lv
