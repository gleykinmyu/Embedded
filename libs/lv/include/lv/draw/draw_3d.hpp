#pragma once

/**
 * @file draw_3d.hpp
 * @brief Wrapper for LVGL 3D texture drawing
 *
 * Requires LV_USE_3DTEXTURE enabled in lv_conf.h.
 * Used to render textures from 3D backends (OpenGL, Vulkan, etc.) onto LVGL layers.
 *
 * @note This is typically used internally by the Texture3D widget,
 * but can be used directly for custom 3D rendering integration.
 */

#include <lvgl.h>
#include <src/draw/lv_draw_3d.h>

#if LV_USE_3DTEXTURE

#include "layer.hpp"

namespace lv {

/**
 * @brief Fluent wrapper for lv_draw_3d_dsc_t
 *
 * Used to draw 3D textures (e.g., from OpenGL) onto an LVGL layer.
 *
 * Example:
 * @code
 * // In a custom draw event handler:
 * GLuint gl_texture = ...;
 *
 * lv::Draw3dDsc dsc;
 * dsc.texture(static_cast<lv_3dtexture_id_t>(gl_texture))
 *    .flip_v()  // OpenGL textures often need vertical flip
 *    .opa(LV_OPA_COVER);
 *
 * lv::draw::texture_3d(layer, dsc, coords);
 * @endcode
 */
class Draw3dDsc {
    lv_draw_3d_dsc_t m_dsc{};

public:
    /// Initialize with defaults
    Draw3dDsc() noexcept {
        lv_draw_3d_dsc_init(&m_dsc);
    }

    /// Get the underlying descriptor
    [[nodiscard]] lv_draw_3d_dsc_t* get() noexcept { return &m_dsc; }
    [[nodiscard]] const lv_draw_3d_dsc_t* get() const noexcept { return &m_dsc; }

    /// Implicit conversion for LVGL API
    operator const lv_draw_3d_dsc_t*() const noexcept { return &m_dsc; }

    // ==================== Texture ====================

    /**
     * @brief Set the texture ID
     *
     * The texture ID is backend-specific:
     * - OpenGL: GLuint texture handle
     * - Vulkan: VkImage or similar
     *
     * @param id Texture handle from 3D backend
     */
    Draw3dDsc& texture(lv_3dtexture_id_t id) noexcept {
        m_dsc.tex_id = id;
        return *this;
    }

    // ==================== Flipping ====================

    /**
     * @brief Set horizontal and vertical flip
     * @param h Flip horizontally
     * @param v Flip vertically (often needed for OpenGL textures)
     */
    Draw3dDsc& flip(bool h, bool v) noexcept {
        m_dsc.h_flip = h;
        m_dsc.v_flip = v;
        return *this;
    }

    /// Flip horizontally
    Draw3dDsc& flip_h(bool flip = true) noexcept {
        m_dsc.h_flip = flip;
        return *this;
    }

    /// Flip vertically (common for OpenGL textures)
    Draw3dDsc& flip_v(bool flip = true) noexcept {
        m_dsc.v_flip = flip;
        return *this;
    }

    // ==================== Opacity ====================

    /// Set opacity
    Draw3dDsc& opa(lv_opa_t o) noexcept {
        m_dsc.opa = o;
        return *this;
    }
};

// ==================== Draw Function ====================

namespace draw {

/**
 * @brief Draw a 3D texture onto a layer
 *
 * @param layer Target layer
 * @param dsc   Draw descriptor with texture ID and settings
 * @param coords Area where the texture will be drawn
 */
inline void texture_3d(lv_layer_t* layer, const Draw3dDsc& dsc, const lv_area_t& coords) noexcept {
    lv_draw_3d(layer, dsc.get(), &coords);
}

} // namespace draw

} // namespace lv

#endif // LV_USE_3DTEXTURE
