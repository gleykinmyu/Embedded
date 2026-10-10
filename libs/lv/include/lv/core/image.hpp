#pragma once

/**
 * @file image.hpp
 * @brief Image loading and management utilities for LVGL
 *
 * Provides C++ wrappers for image loading, including:
 * - File-based images (PNG, JPG, BMP, etc.)
 * - Memory-based images (embedded C arrays)
 * - Symbol images
 */

#include <lvgl.h>
#include <cstdint>
#include <cstring>

namespace lv {

// ==================== Image Source Types ====================

namespace image_src {
    /// Check if source is a variable (C array)
    inline bool is_variable(const void* src) noexcept {
        return lv_image_src_get_type(src) == LV_IMAGE_SRC_VARIABLE;
    }

    /// Check if source is a file path
    inline bool is_file(const void* src) noexcept {
        return lv_image_src_get_type(src) == LV_IMAGE_SRC_FILE;
    }

    /// Check if source is a symbol
    inline bool is_symbol(const void* src) noexcept {
        return lv_image_src_get_type(src) == LV_IMAGE_SRC_SYMBOL;
    }
} // namespace image_src

// ==================== Image Alignment ====================

#if LV_USE_IMAGE
namespace image_align {
    constexpr auto top_left = LV_IMAGE_ALIGN_TOP_LEFT;
    constexpr auto top_mid = LV_IMAGE_ALIGN_TOP_MID;
    constexpr auto top_right = LV_IMAGE_ALIGN_TOP_RIGHT;
    constexpr auto bottom_left = LV_IMAGE_ALIGN_BOTTOM_LEFT;
    constexpr auto bottom_mid = LV_IMAGE_ALIGN_BOTTOM_MID;
    constexpr auto bottom_right = LV_IMAGE_ALIGN_BOTTOM_RIGHT;
    constexpr auto left_mid = LV_IMAGE_ALIGN_LEFT_MID;
    constexpr auto right_mid = LV_IMAGE_ALIGN_RIGHT_MID;
    constexpr auto center = LV_IMAGE_ALIGN_CENTER;
    constexpr auto stretch = LV_IMAGE_ALIGN_STRETCH;
    constexpr auto tile = LV_IMAGE_ALIGN_TILE;
    constexpr auto contain = LV_IMAGE_ALIGN_CONTAIN;
    constexpr auto cover = LV_IMAGE_ALIGN_COVER;
} // namespace image_align
#endif // LV_USE_IMAGE

// ==================== Image Descriptor Wrapper ====================

/**
 * @brief Image descriptor information
 *
 * Wraps lv_image_dsc_t for convenient access to image properties.
 */
class ImageInfo {
    const lv_image_dsc_t* m_dsc;

public:
    explicit ImageInfo(const lv_image_dsc_t* dsc) noexcept : m_dsc(dsc) {}

    /// Get underlying descriptor
    [[nodiscard]] const lv_image_dsc_t* get() const noexcept { return m_dsc; }

    /// Check if valid
    [[nodiscard]] bool valid() const noexcept { return m_dsc != nullptr; }
    explicit operator bool() const noexcept { return valid(); }

    /// Get image width
    [[nodiscard]] uint32_t width() const noexcept {
        return m_dsc ? m_dsc->header.w : 0;
    }

    /// Get image height
    [[nodiscard]] uint32_t height() const noexcept {
        return m_dsc ? m_dsc->header.h : 0;
    }

    /// Get color format
    [[nodiscard]] lv_color_format_t color_format() const noexcept {
        return m_dsc ? static_cast<lv_color_format_t>(m_dsc->header.cf) : LV_COLOR_FORMAT_UNKNOWN;
    }

    /// Get stride (bytes per row)
    [[nodiscard]] uint32_t stride() const noexcept {
        return m_dsc ? m_dsc->header.stride : 0;
    }

    /// Get data pointer
    [[nodiscard]] const uint8_t* data() const noexcept {
        return m_dsc ? m_dsc->data : nullptr;
    }

    /// Get data size
    [[nodiscard]] uint32_t data_size() const noexcept {
        return m_dsc ? m_dsc->data_size : 0;
    }
};

// ==================== Image Header Utilities ====================

/**
 * @brief Get image header information from any source
 *
 * Works with file paths, C arrays, or symbols.
 */
inline bool get_image_header(const void* src, lv_image_header_t* header) noexcept {
    return lv_image_decoder_get_info(src, header) == LV_RESULT_OK;
}

/**
 * @brief Get image dimensions from any source
 */
struct ImageSize {
    uint32_t width = 0;
    uint32_t height = 0;
    bool valid = false;
};

inline ImageSize get_image_size(const void* src) noexcept {
    lv_image_header_t header;
    if (lv_image_decoder_get_info(src, &header) == LV_RESULT_OK) {
        return {header.w, header.h, true};
    }
    return {0, 0, false};
}

// ==================== File Path Helpers ====================

/**
 * @brief Build LVGL file path with drive letter
 *
 * LVGL uses drive letters like "A:/path/to/file.png"
 * Common drive letters:
 * - 'A' or '/' for POSIX filesystem (LV_USE_FS_POSIX)
 * - 'S' for STDIO filesystem (LV_USE_FS_STDIO)
 *
 * @param path The file path
 * @param drive_letter The drive letter (default '/' for POSIX)
 * @return The formatted path string
 */
class FilePath {
    char m_path[LV_FS_MAX_PATH_LENGTH];

public:
    /// Create from path with auto-detected drive letter
    explicit FilePath(const char* path) noexcept {
        // If path already has drive letter format, use as-is
        if (path && path[0] && path[1] == ':') {
            lv_snprintf(m_path, sizeof(m_path), "%s", path);
        }
        // Otherwise, assume POSIX-style path with default drive
        else if (path && path[0] == '/') {
            // Use 'A' drive for absolute paths
            lv_snprintf(m_path, sizeof(m_path), "A:%s", path);
        }
        else if (path) {
            // Relative path - just copy
            lv_snprintf(m_path, sizeof(m_path), "%s", path);
        }
        else {
            m_path[0] = '\0';
        }
    }

    /// Create with explicit drive letter
    FilePath(char drive, const char* path) noexcept {
        if (path) {
            lv_snprintf(m_path, sizeof(m_path), "%c:%s", drive, path);
        } else {
            m_path[0] = '\0';
        }
    }

    /// Get the path string
    [[nodiscard]] const char* c_str() const noexcept { return m_path; }
    [[nodiscard]] const char* get() const noexcept { return m_path; }

    /// Implicit conversion to const char*
    operator const char*() const noexcept { return m_path; }
};

// ==================== Image Loading Helpers ====================

/**
 * @brief Check if an image file exists and is readable
 */
inline bool image_file_exists(const char* path) noexcept {
    lv_image_header_t header;
    return lv_image_decoder_get_info(path, &header) == LV_RESULT_OK;
}

/**
 * @brief Get the file extension from a path
 */
inline const char* get_file_extension(const char* path) noexcept {
    if (!path) return nullptr;
    const char* dot = strrchr(path, '.');
    return dot ? dot + 1 : nullptr;
}

// ==================== Color Format Helpers ====================

namespace color_format {
    constexpr auto unknown = LV_COLOR_FORMAT_UNKNOWN;
    constexpr auto raw = LV_COLOR_FORMAT_RAW;
    constexpr auto raw_alpha = LV_COLOR_FORMAT_RAW_ALPHA;

    constexpr auto l8 = LV_COLOR_FORMAT_L8;
    constexpr auto a8 = LV_COLOR_FORMAT_A8;
    constexpr auto i1 = LV_COLOR_FORMAT_I1;
    constexpr auto i2 = LV_COLOR_FORMAT_I2;
    constexpr auto i4 = LV_COLOR_FORMAT_I4;
    constexpr auto i8 = LV_COLOR_FORMAT_I8;

    constexpr auto rgb565 = LV_COLOR_FORMAT_RGB565;
    constexpr auto rgb888 = LV_COLOR_FORMAT_RGB888;
    constexpr auto argb8888 = LV_COLOR_FORMAT_ARGB8888;
    constexpr auto xrgb8888 = LV_COLOR_FORMAT_XRGB8888;

    /// Get bits per pixel for a color format
    inline uint8_t get_bpp(lv_color_format_t cf) noexcept {
        return lv_color_format_get_bpp(cf);
    }

    /// Check if format has alpha channel
    inline bool has_alpha(lv_color_format_t cf) noexcept {
        return lv_color_format_has_alpha(cf);
    }
} // namespace color_format

// ==================== Scale Constants ====================

namespace scale {
    constexpr uint32_t none = LV_SCALE_NONE;      // 256 = 100%
    constexpr uint32_t half = 128;                // 50%
    constexpr uint32_t double_size = 512;         // 200%
    constexpr uint32_t quarter = 64;              // 25%

    /// Convert percentage to scale value (100 = no scale)
    constexpr uint32_t from_percent(uint32_t pct) noexcept {
        return (pct * LV_SCALE_NONE) / 100;
    }

    /// Convert scale value to percentage
    constexpr uint32_t to_percent(uint32_t scale) noexcept {
        return (scale * 100) / LV_SCALE_NONE;
    }
} // namespace scale

} // namespace lv
