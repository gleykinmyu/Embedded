#pragma once

/**
 * @file image_decoder.hpp
 * @brief C++ wrapper for LVGL image decoder API
 *
 * Provides:
 * - ImageDecoderDsc: RAII wrapper for decoding sessions (open/decode/close)
 * - ImageDecoder: Custom decoder registration with C++ callbacks
 * - Helper functions for getting image info
 *
 * @note Image decoders are used internally by LVGL for PNG, JPG, BMP, etc.
 * You can create custom decoders for proprietary formats.
 */

#include <lvgl.h>
#include <src/draw/lv_image_decoder_private.h>  // For full struct definitions

namespace lv {

// ==================== Image Source Helpers ====================

/**
 * @brief Get the type of an image source
 * @param src Image source (file path, lv_image_dsc_t*, or symbol)
 * @return Source type
 */
inline lv_image_src_t image_src_type(const void* src) noexcept {
    return lv_image_src_get_type(src);
}

/**
 * @brief Get image info without decoding
 *
 * @param src Image source
 * @param header Output: image header with dimensions, color format, etc.
 * @return LV_RESULT_OK on success
 */
inline lv_result_t image_get_info(const void* src, lv_image_header_t& header) noexcept {
    return lv_image_decoder_get_info(src, &header);
}

// ==================== Decode Session (RAII) ====================

/**
 * @brief RAII wrapper for image decoding session
 *
 * Automatically closes the decoding session when destroyed.
 * Use this for decoding images in custom draw code.
 *
 * Example:
 * @code
 * // Decode an image
 * lv::ImageDecoderDsc dsc;
 * if (dsc.open("A:image.png")) {
 *     // Access decoded pixels
 *     const lv_draw_buf_t* buf = dsc.decoded();
 *     uint32_t w = dsc.header().w;
 *     uint32_t h = dsc.header().h;
 *
 *     // Use the pixel data...
 * }
 * // Automatically closed when dsc goes out of scope
 * @endcode
 */
class ImageDecoderDsc {
    lv_image_decoder_dsc_t m_dsc{};
    bool m_open = false;

public:
    ImageDecoderDsc() noexcept = default;

    /// Non-copyable
    ImageDecoderDsc(const ImageDecoderDsc&) = delete;
    ImageDecoderDsc& operator=(const ImageDecoderDsc&) = delete;

    /// Movable
    ImageDecoderDsc(ImageDecoderDsc&& other) noexcept
        : m_dsc(other.m_dsc), m_open(other.m_open) {
        other.m_open = false;
    }

    ImageDecoderDsc& operator=(ImageDecoderDsc&& other) noexcept {
        if (this != &other) {
            close();
            m_dsc = other.m_dsc;
            m_open = other.m_open;
            other.m_open = false;
        }
        return *this;
    }

    /// Destructor - closes session if open
    ~ImageDecoderDsc() {
        close();
    }

    /// Get underlying descriptor
    [[nodiscard]] lv_image_decoder_dsc_t* get() noexcept { return &m_dsc; }
    [[nodiscard]] const lv_image_decoder_dsc_t* get() const noexcept { return &m_dsc; }

    // ==================== Session Management ====================

    /**
     * @brief Open an image for decoding
     *
     * @param src Image source (file path or lv_image_dsc_t*)
     * @param args Optional decode arguments (nullptr for defaults)
     * @return true if successfully opened
     */
    [[nodiscard]] bool open(const void* src, const lv_image_decoder_args_t* args = nullptr) noexcept {
        close();  // Close any previous session
        lv_result_t res = lv_image_decoder_open(&m_dsc, src, args);
        m_open = (res == LV_RESULT_OK);
        return m_open;
    }

    /**
     * @brief Open with specific decode options
     *
     * @param src Image source
     * @param premultiply Whether to premultiply alpha
     * @param no_cache Bypass image cache
     * @return true if successfully opened
     */
    [[nodiscard]] bool open(const void* src, bool premultiply, bool no_cache = false) noexcept {
        lv_image_decoder_args_t args{};
        args.premultiply = premultiply;
        args.no_cache = no_cache;
        return open(src, &args);
    }

    /**
     * @brief Decode a specific area (for progressive decoding)
     *
     * @param full_area The full area to decode
     * @param decoded_area Input/output: set to LV_COORD_MIN initially, returns decoded area
     * @return LV_RESULT_OK if more data available, LV_RESULT_INVALID when done
     */
    [[nodiscard]] lv_result_t get_area(const lv_area_t& full_area, lv_area_t& decoded_area) noexcept {
        return lv_image_decoder_get_area(&m_dsc, &full_area, &decoded_area);
    }

    /// Close the decoding session
    void close() noexcept {
        if (m_open) {
            lv_image_decoder_close(&m_dsc);
            m_open = false;
        }
    }

    /// Check if session is open
    [[nodiscard]] bool is_open() const noexcept { return m_open; }
    explicit operator bool() const noexcept { return m_open; }

    // ==================== Accessors ====================

    /// Get image header (dimensions, color format, etc.)
    [[nodiscard]] const lv_image_header_t& header() const noexcept { return m_dsc.header; }

    /// Get decoded image buffer (nullptr if not decoded yet)
    [[nodiscard]] const lv_draw_buf_t* decoded() const noexcept { return m_dsc.decoded; }

    /// Get image width
    [[nodiscard]] uint32_t width() const noexcept { return m_dsc.header.w; }

    /// Get image height
    [[nodiscard]] uint32_t height() const noexcept { return m_dsc.header.h; }

    /// Get color format
    [[nodiscard]] lv_color_format_t color_format() const noexcept {
        return static_cast<lv_color_format_t>(m_dsc.header.cf);
    }

    /// Get source type
    [[nodiscard]] lv_image_src_t src_type() const noexcept { return m_dsc.src_type; }

    /// Get error message (if decode failed)
    [[nodiscard]] const char* error_msg() const noexcept { return m_dsc.error_msg; }

    /// Get palette (for indexed images)
    [[nodiscard]] const lv_color32_t* palette() const noexcept { return m_dsc.palette; }

    /// Get palette size
    [[nodiscard]] uint32_t palette_size() const noexcept { return m_dsc.palette_size; }
};

// ==================== Custom Decoder ====================

/**
 * @brief Custom image decoder wrapper
 *
 * Use this to register custom image decoders for proprietary formats.
 * The decoder is automatically deleted when this object is destroyed.
 *
 * Example:
 * @code
 * // Create a custom decoder for ".myimg" files
 * static lv::ImageDecoder my_decoder;
 *
 * void init_my_decoder() {
 *     my_decoder.create();
 *     my_decoder.set_info_cb([](lv_image_decoder_t*, lv_image_decoder_dsc_t* dsc,
 *                               lv_image_header_t* header) -> lv_result_t {
 *         // Check if we can handle this source
 *         if (dsc->src_type != LV_IMAGE_SRC_FILE) return LV_RESULT_INVALID;
 *
 *         const char* fn = (const char*)dsc->src;
 *         if (!has_extension(fn, ".myimg")) return LV_RESULT_INVALID;
 *
 *         // Fill in header
 *         header->w = ...;
 *         header->h = ...;
 *         header->cf = LV_COLOR_FORMAT_ARGB8888;
 *         return LV_RESULT_OK;
 *     });
 *
 *     my_decoder.set_open_cb([](lv_image_decoder_t*, lv_image_decoder_dsc_t* dsc) -> lv_result_t {
 *         // Decode the image...
 *         return LV_RESULT_OK;
 *     });
 * }
 * @endcode
 */
class ImageDecoder {
    lv_image_decoder_t* m_decoder = nullptr;

public:
    ImageDecoder() noexcept = default;

    /// Non-copyable
    ImageDecoder(const ImageDecoder&) = delete;
    ImageDecoder& operator=(const ImageDecoder&) = delete;

    /// Movable
    ImageDecoder(ImageDecoder&& other) noexcept : m_decoder(other.m_decoder) {
        other.m_decoder = nullptr;
    }

    ImageDecoder& operator=(ImageDecoder&& other) noexcept {
        if (this != &other) {
            destroy();
            m_decoder = other.m_decoder;
            other.m_decoder = nullptr;
        }
        return *this;
    }

    /// Destructor - deletes the decoder
    ~ImageDecoder() {
        destroy();
    }

    /// Get underlying decoder pointer
    [[nodiscard]] lv_image_decoder_t* get() noexcept { return m_decoder; }
    [[nodiscard]] const lv_image_decoder_t* get() const noexcept { return m_decoder; }

    /// Check if decoder is created
    [[nodiscard]] bool valid() const noexcept { return m_decoder != nullptr; }
    explicit operator bool() const noexcept { return valid(); }

    // ==================== Lifecycle ====================

    /// Create the decoder
    [[nodiscard]] bool create() noexcept {
        destroy();
        m_decoder = lv_image_decoder_create();
        return m_decoder != nullptr;
    }

    /// Destroy the decoder
    void destroy() noexcept {
        if (m_decoder) {
            lv_image_decoder_delete(m_decoder);
            m_decoder = nullptr;
        }
    }

    // ==================== Callback Setters ====================

    /**
     * @brief Set info callback
     *
     * Called to check if decoder can handle a source and fill header info.
     * Return LV_RESULT_OK if handled, LV_RESULT_INVALID to try next decoder.
     */
    ImageDecoder& set_info_cb(lv_image_decoder_info_f_t cb) noexcept {
        if (m_decoder) lv_image_decoder_set_info_cb(m_decoder, cb);
        return *this;
    }

    /**
     * @brief Set open callback
     *
     * Called to decode the image. Should set dsc->decoded with decoded pixels.
     */
    ImageDecoder& set_open_cb(lv_image_decoder_open_f_t cb) noexcept {
        if (m_decoder) lv_image_decoder_set_open_cb(m_decoder, cb);
        return *this;
    }

    /**
     * @brief Set get_area callback (for progressive decoding)
     *
     * Optional. Called to decode image in chunks.
     */
    ImageDecoder& set_get_area_cb(lv_image_decoder_get_area_cb_t cb) noexcept {
        if (m_decoder) lv_image_decoder_set_get_area_cb(m_decoder, cb);
        return *this;
    }

    /**
     * @brief Set close callback
     *
     * Called to clean up resources after decoding.
     */
    ImageDecoder& set_close_cb(lv_image_decoder_close_f_t cb) noexcept {
        if (m_decoder) lv_image_decoder_set_close_cb(m_decoder, cb);
        return *this;
    }

    // ==================== User Data ====================

    /// Set user data (accessible in callbacks)
    ImageDecoder& user_data(void* data) noexcept {
        if (m_decoder) m_decoder->user_data = data;
        return *this;
    }

    /// Get user data
    [[nodiscard]] void* user_data() const noexcept {
        return m_decoder ? m_decoder->user_data : nullptr;
    }
};

// ==================== Decoder Iteration ====================

/**
 * @brief Iterate over registered decoders
 *
 * @param prev Previous decoder (nullptr to get first)
 * @return Next decoder or nullptr
 */
inline lv_image_decoder_t* image_decoder_next(lv_image_decoder_t* prev = nullptr) noexcept {
    return lv_image_decoder_get_next(prev);
}

} // namespace lv
