#pragma once

/**
 * @file translation.hpp
 * @brief Internationalization (i18n) support for LVGL
 *
 * Provides C++ wrappers for LVGL's translation system.
 * Requires LV_USE_TRANSLATION to be enabled.
 *
 * Usage:
 *   // Define translations
 *   static const char* const langs[] = {"en", "de", nullptr};
 *   static const char* const tags[] = {"hello", "goodbye", nullptr};
 *   static const char* const trans[] = {
 *       "Hello", "Hallo",
 *       "Goodbye", "Auf Wiedersehen",
 *   };
 *   lv::translation::add_static(langs, tags, trans);
 *
 *   // Set language and use
 *   lv::translation::set_language("de");
 *   label.text(lv::tr("hello"));  // Shows "Hallo"
 */

#include <lvgl.h>

#if LV_USE_TRANSLATION

#include <cstdint>

namespace lv {

// ==================== Shorthand Translation Function ====================

/**
 * @brief Get translated text for a tag
 *
 * Shorthand for lv_tr(). Returns the translation for the current language.
 *
 * @param tag The tag/key to translate
 * @return Translated text, or the tag itself if not found
 */
inline const char* tr(const char* tag) noexcept {
    return lv_tr(tag);
}

// ==================== Translation Namespace ====================

namespace translation {

/**
 * @brief Initialize the translation module
 */
inline void init() noexcept {
    lv_translation_init();
}

/**
 * @brief Deinitialize the translation module
 */
inline void deinit() noexcept {
    lv_translation_deinit();
}

/**
 * @brief Set the current language
 *
 * Sends LV_EVENT_TRANSLATION_LANGUAGE_CHANGED to all widgets.
 *
 * @param lang Language code (e.g., "en", "de", "fr")
 */
inline void set_language(const char* lang) noexcept {
    lv_translation_set_language(lang);
}

/**
 * @brief Get the current language
 * @return Current language code
 */
inline const char* get_language() noexcept {
    return lv_translation_get_language();
}

/**
 * @brief Get translated text for a tag
 * @param tag The tag/key to translate
 * @return Translated text
 */
inline const char* get(const char* tag) noexcept {
    return lv_translation_get(tag);
}

/**
 * @brief Add a static translation pack
 *
 * All arrays must be static/constant and remain valid while in use.
 *
 * @param languages Null-terminated array of language codes: {"en", "de", nullptr}
 * @param tags Null-terminated array of tag names: {"hello", "goodbye", nullptr}
 * @param translations Flat array of translations, ordered by tag then language
 * @return The created translation pack
 *
 * Example:
 *   static const char* const langs[] = {"en", "de", nullptr};
 *   static const char* const tags[] = {"hello", "bye", nullptr};
 *   static const char* const trans[] = {
 *       "Hello", "Hallo",      // "hello" in en, de
 *       "Goodbye", "Tschüss",  // "bye" in en, de
 *   };
 *   lv::translation::add_static(langs, tags, trans);
 */
inline lv_translation_pack_t* add_static(
    const char* languages[],
    const char* tags[],
    const char* translations[]) noexcept {
    return lv_translation_add_static(languages, tags, translations);
}

// ==================== Dynamic Translation Pack ====================

/**
 * @brief RAII wrapper for dynamic translation packs
 *
 * Allows adding languages and translations at runtime.
 *
 * Usage:
 *   lv::translation::DynamicPack pack;
 *   pack.add_language("en");
 *   pack.add_language("de");
 *   pack.add_translation("hello", {"Hello", "Hallo"});
 */
class DynamicPack {
    lv_translation_pack_t* m_pack;

public:
    DynamicPack() noexcept : m_pack(lv_translation_add_dynamic()) {}

    // Non-copyable, non-moveable (pack is managed by LVGL)
    DynamicPack(const DynamicPack&) = delete;
    DynamicPack& operator=(const DynamicPack&) = delete;
    DynamicPack(DynamicPack&&) = delete;
    DynamicPack& operator=(DynamicPack&&) = delete;

    /// Get underlying pack pointer
    [[nodiscard]] lv_translation_pack_t* get() noexcept { return m_pack; }

    /// Check if valid
    [[nodiscard]] bool valid() const noexcept { return m_pack != nullptr; }
    explicit operator bool() const noexcept { return valid(); }

    /**
     * @brief Add a language to the pack
     *
     * Must be called before adding any tags.
     *
     * @param lang Language code (e.g., "en", "de")
     * @return true on success
     */
    [[nodiscard]] bool add_language(const char* lang) noexcept {
        return m_pack && lv_translation_add_language(m_pack, lang) == LV_RESULT_OK;
    }

    /**
     * @brief Get index of a language in this pack
     * @param lang Language code
     * @return Index or -1 if not found
     */
    [[nodiscard]] int32_t get_language_index(const char* lang) const noexcept {
        return m_pack ? lv_translation_get_language_index(m_pack, lang) : -1;
    }

    /**
     * @brief Add a tag with its translations
     *
     * @param tag_name The tag/key name
     * @param translations Array of translations, one per language (in order added)
     * @return true on success
     */
    template<size_t N>
    [[nodiscard]] bool add_translation(const char* tag_name, const char* const (&translations)[N]) noexcept {
        if (!m_pack) return false;

        lv_translation_tag_dsc_t* tag = lv_translation_add_tag(m_pack, tag_name);
        if (!tag) return false;

        for (size_t i = 0; i < N; ++i) {
            if (lv_translation_set_tag_translation(m_pack, tag, static_cast<uint32_t>(i),
                                                    translations[i]) != LV_RESULT_OK) {
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Add a tag and set translations individually
     * @param tag_name The tag/key name
     * @return Tag descriptor for setting translations
     */
    [[nodiscard]] lv_translation_tag_dsc_t* add_tag(const char* tag_name) noexcept {
        return m_pack ? lv_translation_add_tag(m_pack, tag_name) : nullptr;
    }

    /**
     * @brief Set translation for a specific tag and language
     * @param tag Tag descriptor from add_tag()
     * @param lang_index Language index (order added)
     * @param text Translation text
     * @return true on success
     */
    bool set_translation(lv_translation_tag_dsc_t* tag, uint32_t lang_index,
                         const char* text) noexcept {
        return m_pack && tag &&
               lv_translation_set_tag_translation(m_pack, tag, lang_index, text) == LV_RESULT_OK;
    }
};

} // namespace translation

} // namespace lv

#endif // LV_USE_TRANSLATION
