#pragma once

/**
 * @file svg.hpp
 * @brief Zero-cost wrapper for LVGL SVG rendering
 *
 * Provides RAII wrappers for SVG DOM parsing and rendering.
 *
 * Usage:
 *   lv::svg::Node doc(svg_data, data_len);
 *   lv::svg::draw(layer, doc);
 */

#include <lvgl.h>
#include <cstring>

#if LV_USE_SVG

namespace lv::svg {

/**
 * @brief RAII wrapper for an SVG DOM tree
 *
 * Parses SVG data into a DOM tree. Deletes on destruction.
 *
 * Size: sizeof(void*) — pointer-sized
 */
class Node {
    lv_svg_node_t* m_node = nullptr;

public:
    /// Parse SVG data into a DOM tree
    Node(const char* svg_data, uint32_t data_len) noexcept
        : m_node(lv_svg_load_data(svg_data, data_len)) {}

    /// Parse SVG data (null-terminated string, auto-computes length)
    explicit Node(const char* svg_data) noexcept
        : m_node(svg_data ? lv_svg_load_data(svg_data, static_cast<uint32_t>(std::strlen(svg_data))) : nullptr) {}

    /// Adopt an existing node
    struct adopt_t {};
    static constexpr adopt_t adopt{};
    Node(adopt_t, lv_svg_node_t* node) noexcept : m_node(node) {}

    ~Node() noexcept {
        if (m_node) lv_svg_node_delete(m_node);
    }

    // Move-only
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    Node(Node&& other) noexcept : m_node(other.m_node) {
        other.m_node = nullptr;
    }

    Node& operator=(Node&& other) noexcept {
        if (this != &other) {
            if (m_node) lv_svg_node_delete(m_node);
            m_node = other.m_node;
            other.m_node = nullptr;
        }
        return *this;
    }

    /// Get underlying node pointer
    [[nodiscard]] lv_svg_node_t* get() noexcept { return m_node; }
    [[nodiscard]] const lv_svg_node_t* get() const noexcept { return m_node; }

    /// Check if valid
    [[nodiscard]] explicit operator bool() const noexcept { return m_node != nullptr; }
};

/**
 * @brief RAII wrapper for a compiled SVG render object
 *
 * Created from a Node, used for efficient repeated rendering.
 *
 * Size: sizeof(void*) — pointer-sized
 */
class RenderObj {
    lv_svg_render_obj_t* m_render = nullptr;

public:
    /// Create render object from an SVG DOM tree
    explicit RenderObj(const Node& doc) noexcept
        : m_render(lv_svg_render_create(doc.get())) {}

    /// Adopt an existing render object
    struct adopt_t {};
    static constexpr adopt_t adopt{};
    RenderObj(adopt_t, lv_svg_render_obj_t* render) noexcept : m_render(render) {}

    ~RenderObj() noexcept {
        if (m_render) lv_svg_render_delete(m_render);
    }

    // Move-only
    RenderObj(const RenderObj&) = delete;
    RenderObj& operator=(const RenderObj&) = delete;

    RenderObj(RenderObj&& other) noexcept : m_render(other.m_render) {
        other.m_render = nullptr;
    }

    RenderObj& operator=(RenderObj&& other) noexcept {
        if (this != &other) {
            if (m_render) lv_svg_render_delete(m_render);
            m_render = other.m_render;
            other.m_render = nullptr;
        }
        return *this;
    }

    // ==================== Getters ====================

    /// Get total memory size of render objects
    [[nodiscard]] uint32_t size() const noexcept {
        return lv_svg_render_get_size(m_render);
    }

    /// Get SVG viewport dimensions
    [[nodiscard]] bool viewport_size(float& width, float& height) const noexcept {
        return lv_svg_render_get_viewport_size(m_render, &width, &height) == LV_RESULT_OK;
    }

    /// Render to a vector graphics descriptor
    void draw(lv_draw_vector_dsc_t* dsc) const noexcept {
        lv_draw_svg_render(dsc, m_render);
    }

    /// Get underlying render object pointer
    [[nodiscard]] lv_svg_render_obj_t* get() noexcept { return m_render; }
    [[nodiscard]] const lv_svg_render_obj_t* get() const noexcept { return m_render; }

    /// Check if valid
    [[nodiscard]] explicit operator bool() const noexcept { return m_render != nullptr; }
};

// ==================== Free Functions ====================

/// Initialize SVG renderer with HAL callbacks
inline void init(const lv_svg_render_hal_t* hal) noexcept {
    lv_svg_render_init(hal);
}

/// Draw an SVG document directly to a layer (convenience, parses + renders)
inline void draw(lv_layer_t* layer, const Node& doc) noexcept {
    lv_draw_svg(layer, doc.get());
}

} // namespace lv::svg

#endif // LV_USE_SVG
