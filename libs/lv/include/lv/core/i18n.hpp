#pragma once

/**
 * @file i18n.hpp
 * @brief C++ wrapper for lv_i18n addon (internationalization)
 *
 * This wraps the external lv_i18n addon, not the built-in LVGL translation API.
 * Include this header after including the lv_i18n.h header from your translations.
 *
 * @note This header is NOT included by lv.hpp - you must include it explicitly
 * after including your generated lv_i18n.h translations header.
 */

// Guard: only compile if lv_i18n types are defined
#if defined(LV_I18N_H) || defined(_LV_I18N_H)

namespace lv {

/**
 * @brief Internationalization utilities for lv_i18n addon
 *
 * Wraps the lv_i18n addon functions for locale management.
 * Requires the lv_i18n addon to be present and its types defined.
 *
 * Usage:
 * @code
 * #include "translations/lv_i18n.h"  // Generated translations
 * #include <lv/core/i18n.hpp>
 *
 * // Initialize with language pack (typically generated)
 * lv::i18n::init(lv_i18n_language_pack);
 *
 * // Set locale
 * lv::i18n::set_locale("en");
 * lv::i18n::set_locale("zh");
 *
 * // Use _() macro for translations (from lv_i18n.h)
 * label.text(_("Hello"));
 * @endcode
 */
namespace i18n {

/// Initialize i18n with a language pack
inline void init(const lv_i18n_language_pack_t* pack) noexcept {
    lv_i18n_init(pack);
}

/// Set the current locale
inline void set_locale(const char* locale) noexcept {
    lv_i18n_set_locale(locale);
}

/// Get the current locale
[[nodiscard]] inline const char* get_locale() noexcept {
    return lv_i18n_get_current_locale();
}

} // namespace i18n

} // namespace lv

#endif // LV_I18N_H
