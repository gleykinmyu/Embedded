#pragma once

/**
 * @file font_loader.hpp
 * @brief Runtime font loading utilities for LVGL
 *
 * Provides C++ wrappers for loading fonts at runtime:
 * - FreeType fonts (requires LV_USE_FREETYPE)
 * - TinyTTF fonts (requires LV_USE_TINY_TTF)
 *
 * Both backends support TTF/OTF fonts loaded from files or memory.
 */

#include <lvgl.h>
#include <cstdint>
#include <cstddef>

namespace lv {

// ==================== Font Style ====================

#if LV_USE_FREETYPE
namespace font_style {
    constexpr auto normal = LV_FREETYPE_FONT_STYLE_NORMAL;
    constexpr auto italic = LV_FREETYPE_FONT_STYLE_ITALIC;
    constexpr auto bold = LV_FREETYPE_FONT_STYLE_BOLD;
    constexpr auto bold_italic = static_cast<lv_freetype_font_style_t>(
        LV_FREETYPE_FONT_STYLE_BOLD | LV_FREETYPE_FONT_STYLE_ITALIC);
} // namespace font_style

namespace font_render {
    constexpr auto bitmap = LV_FREETYPE_FONT_RENDER_MODE_BITMAP;
    constexpr auto outline = LV_FREETYPE_FONT_RENDER_MODE_OUTLINE;
} // namespace font_render
#endif

// ==================== FreeType Font ====================

#if LV_USE_FREETYPE

/**
 * @brief Initialize FreeType library
 *
 * Must be called before using FreeType fonts.
 *
 * @param max_glyph_cnt Maximum number of glyphs to cache
 * @return true on success
 */
inline bool freetype_init(uint32_t max_glyph_cnt = 256) noexcept {
    return lv_freetype_init(max_glyph_cnt) == LV_RESULT_OK;
}

/**
 * @brief Uninitialize FreeType library
 */
inline void freetype_uninit() noexcept {
    lv_freetype_uninit();
}

/**
 * @brief RAII wrapper for FreeType fonts
 *
 * Automatically deletes the font when destroyed.
 *
 * Usage:
 *   lv::FreeTypeFont font("/path/to/font.ttf", 24);
 *   if (font) {
 *       label.font(font.get());
 *   }
 */
class FreeTypeFont {
    lv_font_t* m_font = nullptr;
    bool m_owned = true;

public:
    /// Create from file path
    FreeTypeFont(const char* path, uint32_t size,
                 lv_freetype_font_style_t style = LV_FREETYPE_FONT_STYLE_NORMAL,
                 lv_freetype_font_render_mode_t render = LV_FREETYPE_FONT_RENDER_MODE_BITMAP) noexcept
        : m_font(lv_freetype_font_create(path, render, size, style)), m_owned(true) {}

    /// Wrap existing font (non-owning)
    explicit FreeTypeFont(lv_font_t* font) noexcept
        : m_font(font), m_owned(false) {}

    ~FreeTypeFont() {
        if (m_owned && m_font) {
            lv_freetype_font_delete(m_font);
        }
    }

    // Non-copyable
    FreeTypeFont(const FreeTypeFont&) = delete;
    FreeTypeFont& operator=(const FreeTypeFont&) = delete;

    // Moveable
    FreeTypeFont(FreeTypeFont&& other) noexcept
        : m_font(other.m_font), m_owned(other.m_owned) {
        other.m_font = nullptr;
        other.m_owned = false;
    }

    FreeTypeFont& operator=(FreeTypeFont&& other) noexcept {
        if (this != &other) {
            if (m_owned && m_font) {
                lv_freetype_font_delete(m_font);
            }
            m_font = other.m_font;
            m_owned = other.m_owned;
            other.m_font = nullptr;
            other.m_owned = false;
        }
        return *this;
    }

    /// Get underlying font pointer
    [[nodiscard]] lv_font_t* get() noexcept { return m_font; }
    [[nodiscard]] const lv_font_t* get() const noexcept { return m_font; }

    /// Check if font is valid
    [[nodiscard]] bool valid() const noexcept { return m_font != nullptr; }
    explicit operator bool() const noexcept { return valid(); }

    /// Implicit conversion to lv_font_t*
    operator const lv_font_t*() const noexcept { return m_font; }

    /// Check if this is an outline font
    [[nodiscard]] bool is_outline() const noexcept {
        return m_font && lv_freetype_is_outline_font(m_font);
    }

    /// Get outline scale (for outline fonts)
    [[nodiscard]] uint32_t outline_scale() const noexcept {
        return m_font ? lv_freetype_outline_get_scale(m_font) : 0;
    }

    /// Release ownership (font won't be deleted in destructor)
    [[nodiscard]] lv_font_t* release() noexcept {
        m_owned = false;
        return m_font;
    }
};

#endif // LV_USE_FREETYPE

// ==================== TinyTTF Font ====================

#if LV_USE_TINY_TTF

/**
 * @brief RAII wrapper for TinyTTF fonts
 *
 * Lighter weight TTF support compared to FreeType.
 *
 * Usage:
 *   lv::TinyTTFFont font("/path/to/font.ttf", 24);
 *   if (font) {
 *       label.font(font.get());
 *   }
 */
class TinyTTFFont {
    lv_font_t* m_font = nullptr;
    bool m_owned = true;

public:
#if LV_TINY_TTF_FILE_SUPPORT
    /// Create from file path
    explicit TinyTTFFont(const char* path, int32_t size) noexcept
        : m_font(lv_tiny_ttf_create_file(path, size)), m_owned(true) {}

    /// Create from file with extended options
    TinyTTFFont(const char* path, int32_t size,
                lv_font_kerning_t kerning, size_t cache_size) noexcept
        : m_font(lv_tiny_ttf_create_file_ex(path, size, kerning, cache_size)), m_owned(true) {}
#endif

    /// Create from memory data
    TinyTTFFont(const void* data, size_t data_size, int32_t size) noexcept
        : m_font(lv_tiny_ttf_create_data(data, data_size, size)), m_owned(true) {}

    /// Create from memory with extended options
    TinyTTFFont(const void* data, size_t data_size, int32_t size,
                lv_font_kerning_t kerning, size_t cache_size) noexcept
        : m_font(lv_tiny_ttf_create_data_ex(data, data_size, size, kerning, cache_size)), m_owned(true) {}

    /// Wrap existing font (non-owning)
    explicit TinyTTFFont(lv_font_t* font) noexcept
        : m_font(font), m_owned(false) {}

    ~TinyTTFFont() {
        if (m_owned && m_font) {
            lv_tiny_ttf_destroy(m_font);
        }
    }

    // Non-copyable
    TinyTTFFont(const TinyTTFFont&) = delete;
    TinyTTFFont& operator=(const TinyTTFFont&) = delete;

    // Moveable
    TinyTTFFont(TinyTTFFont&& other) noexcept
        : m_font(other.m_font), m_owned(other.m_owned) {
        other.m_font = nullptr;
        other.m_owned = false;
    }

    TinyTTFFont& operator=(TinyTTFFont&& other) noexcept {
        if (this != &other) {
            if (m_owned && m_font) {
                lv_tiny_ttf_destroy(m_font);
            }
            m_font = other.m_font;
            m_owned = other.m_owned;
            other.m_font = nullptr;
            other.m_owned = false;
        }
        return *this;
    }

    /// Get underlying font pointer
    [[nodiscard]] lv_font_t* get() noexcept { return m_font; }
    [[nodiscard]] const lv_font_t* get() const noexcept { return m_font; }

    /// Check if font is valid
    [[nodiscard]] bool valid() const noexcept { return m_font != nullptr; }
    explicit operator bool() const noexcept { return valid(); }

    /// Implicit conversion to lv_font_t*
    operator const lv_font_t*() const noexcept { return m_font; }

    /// Change font size
    TinyTTFFont& set_size(int32_t size) noexcept {
        if (m_font) {
            lv_tiny_ttf_set_size(m_font, size);
        }
        return *this;
    }

    /// Release ownership (font won't be deleted in destructor)
    [[nodiscard]] lv_font_t* release() noexcept {
        m_owned = false;
        return m_font;
    }
};

#endif // LV_USE_TINY_TTF

// ==================== Dynamic Font Class ====================

/**
 * @brief Backend-agnostic RAII wrapper for runtime-loaded fonts
 *
 * Automatically uses the best available backend (FreeType or TinyTTF)
 * and handles cleanup properly. Use this for fonts loaded at runtime
 * from files or memory. For built-in fonts, use lv::Font instead.
 *
 * Usage:
 *   // Load from file (relative path recommended)
 *   lv::DynamicFont font("A:fonts/DejaVuSans.ttf", 16);
 *   if (font) {
 *       lv_obj_set_style_text_font(label, font.get(), 0);
 *   }
 *
 *   // Load from embedded data
 *   lv::DynamicFont font(embedded_ttf_data, sizeof(embedded_ttf_data), 16);
 */
class DynamicFont {
public:
    enum class Backend { None, TinyTTF, FreeType };

private:
    lv_font_t* m_font = nullptr;
    Backend m_backend = Backend::None;

public:
    /// Default constructor (empty font)
    DynamicFont() noexcept = default;

    /**
     * @brief Load font from file
     *
     * Use relative paths for portability (e.g., "fonts/myfont.ttf").
     * For LVGL filesystem, prefix with drive letter (e.g., "A:fonts/myfont.ttf").
     *
     * @param path Path to TTF/OTF file
     * @param size Font size in pixels
     */
    DynamicFont(const char* path, int32_t size) noexcept {
        [[maybe_unused]] bool ok = load_from_file(path, size);
    }

    /**
     * @brief Load font from memory
     *
     * Useful for embedded fonts compiled into the binary.
     *
     * @param data Pointer to TTF/OTF data
     * @param data_size Size of data in bytes
     * @param size Font size in pixels
     */
    DynamicFont(const void* data, size_t data_size, int32_t size) noexcept {
        [[maybe_unused]] bool ok = load_from_memory(data, data_size, size);
    }

    ~DynamicFont() { destroy(); }

    // Non-copyable
    DynamicFont(const DynamicFont&) = delete;
    DynamicFont& operator=(const DynamicFont&) = delete;

    // Moveable
    DynamicFont(DynamicFont&& other) noexcept
        : m_font(other.m_font), m_backend(other.m_backend) {
        other.m_font = nullptr;
        other.m_backend = Backend::None;
    }

    DynamicFont& operator=(DynamicFont&& other) noexcept {
        if (this != &other) {
            destroy();
            m_font = other.m_font;
            m_backend = other.m_backend;
            other.m_font = nullptr;
            other.m_backend = Backend::None;
        }
        return *this;
    }

    /**
     * @brief Load font from file
     * @return true on success
     */
    [[nodiscard]] bool load_from_file([[maybe_unused]] const char* path, [[maybe_unused]] int32_t size) noexcept {
        destroy();

#if LV_USE_FREETYPE
        m_font = lv_freetype_font_create(path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                                         static_cast<uint32_t>(size), LV_FREETYPE_FONT_STYLE_NORMAL);
        if (m_font) {
            m_backend = Backend::FreeType;
            return true;
        }
#endif

#if LV_USE_TINY_TTF && LV_TINY_TTF_FILE_SUPPORT
        m_font = lv_tiny_ttf_create_file(path, size);
        if (m_font) {
            m_backend = Backend::TinyTTF;
            return true;
        }
#endif

        return false;
    }

    /**
     * @brief Load font from memory
     * @return true on success
     */
    [[nodiscard]] bool load_from_memory([[maybe_unused]] const void* data,
                          [[maybe_unused]] size_t data_size,
                          [[maybe_unused]] int32_t size) noexcept {
        destroy();

#if LV_USE_TINY_TTF
        m_font = lv_tiny_ttf_create_data(data, data_size, size);
        if (m_font) {
            m_backend = Backend::TinyTTF;
            return true;
        }
#endif

        return false;
    }

    /// Destroy the font and release resources
    void destroy() noexcept {
        if (!m_font) return;

        switch (m_backend) {
#if LV_USE_TINY_TTF
            case Backend::TinyTTF:
                lv_tiny_ttf_destroy(m_font);
                break;
#endif
#if LV_USE_FREETYPE
            case Backend::FreeType:
                lv_freetype_font_delete(m_font);
                break;
#endif
            default:
                break;
        }
        m_font = nullptr;
        m_backend = Backend::None;
    }

    /// Get underlying font pointer
    [[nodiscard]] lv_font_t* get() noexcept { return m_font; }
    [[nodiscard]] const lv_font_t* get() const noexcept { return m_font; }

    /// Check if font is valid
    [[nodiscard]] bool valid() const noexcept { return m_font != nullptr; }
    explicit operator bool() const noexcept { return valid(); }

    /// Implicit conversion to lv_font_t*
    operator const lv_font_t*() const noexcept { return m_font; }

    /// Get the backend used
    [[nodiscard]] Backend backend() const noexcept { return m_backend; }

    /// Change font size (TinyTTF only)
    [[nodiscard]] bool set_size([[maybe_unused]] int32_t size) noexcept {
#if LV_USE_TINY_TTF
        if (m_font && m_backend == Backend::TinyTTF) {
            lv_tiny_ttf_set_size(m_font, size);
            return true;
        }
#endif
        return false;
    }

    /// Release ownership (font won't be deleted in destructor)
    [[nodiscard]] lv_font_t* release() noexcept {
        lv_font_t* font = m_font;
        m_font = nullptr;
        m_backend = Backend::None;
        return font;
    }

    /// Check if runtime font loading is available
    static constexpr bool available() noexcept {
#if LV_USE_FREETYPE || LV_USE_TINY_TTF
        return true;
#else
        return false;
#endif
    }

    /// Check if file loading is available
    static constexpr bool file_loading_available() noexcept {
#if LV_USE_FREETYPE || (LV_USE_TINY_TTF && LV_TINY_TTF_FILE_SUPPORT)
        return true;
#else
        return false;
#endif
    }
};

// ==================== Font Manager (if available) ====================

#if LV_USE_FONT_MANAGER

/**
 * @brief RAII wrapper for Font Manager
 *
 * Font Manager provides centralized font management with caching.
 */
class FontManager {
    lv_font_manager_t* m_mgr = nullptr;

public:
    /// Create font manager
    FontManager() noexcept : m_mgr(lv_font_manager_create()) {}

    ~FontManager() {
        if (m_mgr) {
            lv_font_manager_delete(m_mgr);
        }
    }

    // Non-copyable, non-moveable (singleton-like)
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;
    FontManager(FontManager&&) = delete;
    FontManager& operator=(FontManager&&) = delete;

    /// Get underlying manager pointer
    [[nodiscard]] lv_font_manager_t* get() noexcept { return m_mgr; }

    /// Check if valid
    [[nodiscard]] bool valid() const noexcept { return m_mgr != nullptr; }
    explicit operator bool() const noexcept { return valid(); }

    /// Add a font path for lookup
    [[nodiscard]] bool add_path(const char* path) noexcept {
        return m_mgr && lv_font_manager_add_path(m_mgr, path);
    }

    /// Create a font from the manager
    [[nodiscard]] lv_font_t* create_font(const char* name, uint32_t size, uint32_t style = 0) noexcept {
        if (!m_mgr) return nullptr;
        return lv_font_manager_create_font(m_mgr, name, size, style);
    }

    /// Delete a font created by this manager
    [[nodiscard]] bool delete_font(lv_font_t* font) noexcept {
        return m_mgr && lv_font_manager_delete_font(m_mgr, font);
    }
};

#endif // LV_USE_FONT_MANAGER

} // namespace lv
