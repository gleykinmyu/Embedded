#pragma once

/**
 * @file draw_vector.hpp
 * @brief C++ wrapper for LVGL vector graphics API
 *
 * Provides RAII wrappers for vector path creation and drawing.
 * Requires LV_USE_VECTOR_GRAPHIC enabled in lv_conf.h.
 *
 * Example:
 * @code
 * // Create a path
 * lv::VectorPath path(LV_VECTOR_PATH_QUALITY_MEDIUM);
 * path.move_to(50, 50)
 *     .line_to(150, 50)
 *     .line_to(100, 150)
 *     .close();
 *
 * // Draw it
 * lv::VectorDsc dsc(layer);
 * dsc.fill_color(lv::palette::red())
 *    .fill_opa(LV_OPA_COVER)
 *    .stroke_color(lv::palette::blue())
 *    .stroke_width(3.0f)
 *    .add_path(path)
 *    .draw();
 * @endcode
 */

#include <lvgl.h>

#if LV_USE_VECTOR_GRAPHIC

#include <src/draw/lv_draw_vector.h>

namespace lv {

// ==================== Forward Declarations ====================

class VectorPath;
class VectorDsc;

// ==================== Helper Types ====================

/// Float point for vector graphics
using FPoint = lv_fpoint_t;

/// Create an FPoint
[[nodiscard]] constexpr FPoint fpoint(float x, float y) noexcept {
    return FPoint{x, y};
}

// ==================== VectorPath ====================

/**
 * @brief RAII wrapper for lv_vector_path_t
 *
 * Represents a vector graphics path containing curves, lines, and shapes.
 * Automatically manages path lifecycle.
 *
 * Example:
 * @code
 * lv::VectorPath path;
 * path.move_to(0, 0)
 *     .line_to(100, 0)
 *     .quad_to(150, 50, 100, 100)
 *     .close();
 * @endcode
 */
class VectorPath {
    lv_vector_path_t* m_path = nullptr;

public:
    /// Create a path with specified quality
    explicit VectorPath(lv_vector_path_quality_t quality = LV_VECTOR_PATH_QUALITY_MEDIUM) noexcept
        : m_path(lv_vector_path_create(quality)) {}

    /// Non-copyable (use copy() for explicit copies)
    VectorPath(const VectorPath&) = delete;
    VectorPath& operator=(const VectorPath&) = delete;

    /// Moveable
    VectorPath(VectorPath&& other) noexcept : m_path(other.m_path) {
        other.m_path = nullptr;
    }

    VectorPath& operator=(VectorPath&& other) noexcept {
        if (this != &other) {
            if (m_path) lv_vector_path_delete(m_path);
            m_path = other.m_path;
            other.m_path = nullptr;
        }
        return *this;
    }

    /// Destructor
    ~VectorPath() {
        if (m_path) lv_vector_path_delete(m_path);
    }

    /// Get underlying pointer
    [[nodiscard]] lv_vector_path_t* get() noexcept { return m_path; }
    [[nodiscard]] const lv_vector_path_t* get() const noexcept { return m_path; }

    /// Check if valid
    [[nodiscard]] bool valid() const noexcept { return m_path != nullptr; }
    explicit operator bool() const noexcept { return valid(); }

    // ==================== Path Operations ====================

    /// Begin a new sub-path at point
    VectorPath& move_to(float x, float y) noexcept {
        FPoint p{x, y};
        lv_vector_path_move_to(m_path, &p);
        return *this;
    }

    VectorPath& move_to(const FPoint& p) noexcept {
        lv_vector_path_move_to(m_path, &p);
        return *this;
    }

    /// Draw a line to point
    VectorPath& line_to(float x, float y) noexcept {
        FPoint p{x, y};
        lv_vector_path_line_to(m_path, &p);
        return *this;
    }

    VectorPath& line_to(const FPoint& p) noexcept {
        lv_vector_path_line_to(m_path, &p);
        return *this;
    }

    /// Quadratic bezier curve (1 control point)
    VectorPath& quad_to(float cx, float cy, float x, float y) noexcept {
        FPoint ctrl{cx, cy};
        FPoint end{x, y};
        lv_vector_path_quad_to(m_path, &ctrl, &end);
        return *this;
    }

    VectorPath& quad_to(const FPoint& ctrl, const FPoint& end) noexcept {
        lv_vector_path_quad_to(m_path, &ctrl, &end);
        return *this;
    }

    /// Cubic bezier curve (2 control points)
    VectorPath& cubic_to(float c1x, float c1y, float c2x, float c2y, float x, float y) noexcept {
        FPoint ctrl1{c1x, c1y};
        FPoint ctrl2{c2x, c2y};
        FPoint end{x, y};
        lv_vector_path_cubic_to(m_path, &ctrl1, &ctrl2, &end);
        return *this;
    }

    VectorPath& cubic_to(const FPoint& ctrl1, const FPoint& ctrl2, const FPoint& end) noexcept {
        lv_vector_path_cubic_to(m_path, &ctrl1, &ctrl2, &end);
        return *this;
    }

    /// Elliptical arc
    VectorPath& arc_to(float rx, float ry, float rotate_angle,
                       bool large_arc, bool clockwise,
                       float x, float y) noexcept {
        FPoint end{x, y};
        lv_vector_path_arc_to(m_path, rx, ry, rotate_angle, large_arc, clockwise, &end);
        return *this;
    }

    /// Close the current sub-path
    VectorPath& close() noexcept {
        lv_vector_path_close(m_path);
        return *this;
    }

    /// Clear all path data
    VectorPath& clear() noexcept {
        lv_vector_path_clear(m_path);
        return *this;
    }

    // ==================== Shape Helpers ====================

    /// Add a rectangle (with optional rounded corners)
    VectorPath& rect(float x, float y, float w, float h, float rx = 0, float ry = 0) noexcept {
        lv_vector_path_append_rectangle(m_path, x, y, w, h, rx, ry);
        return *this;
    }

    /// Add a rectangle from area
    VectorPath& rect(const lv_area_t& area, float rx = 0, float ry = 0) noexcept {
        lv_vector_path_append_rect(m_path, &area, rx, ry);
        return *this;
    }

    /// Add a circle/ellipse
    VectorPath& circle(float cx, float cy, float rx, float ry) noexcept {
        FPoint center{cx, cy};
        lv_vector_path_append_circle(m_path, &center, rx, ry);
        return *this;
    }

    /// Add a circle (equal radii)
    VectorPath& circle(float cx, float cy, float r) noexcept {
        return circle(cx, cy, r, r);
    }

    /// Add an arc
    VectorPath& arc(float cx, float cy, float radius,
                    float start_angle, float sweep, bool pie = false) noexcept {
        FPoint center{cx, cy};
        lv_vector_path_append_arc(m_path, &center, radius, start_angle, sweep, pie);
        return *this;
    }

    /// Append another path
    VectorPath& append(const VectorPath& other) noexcept {
        lv_vector_path_append_path(m_path, other.m_path);
        return *this;
    }

    // ==================== Utilities ====================

    /// Copy from another path
    VectorPath& copy_from(const VectorPath& other) noexcept {
        lv_vector_path_copy(m_path, other.m_path);
        return *this;
    }

    /// Get bounding box
    [[nodiscard]] lv_area_t bounding_box() const noexcept {
        lv_area_t area{};
        lv_vector_path_get_bounding(m_path, &area);
        return area;
    }

    /// Apply matrix transformation to path
    void transform(const lv_matrix_t& matrix) noexcept {
        lv_matrix_transform_path(&matrix, m_path);
    }
};

// ==================== VectorDsc ====================

/**
 * @brief RAII wrapper for lv_draw_vector_dsc_t
 *
 * Fluent builder for vector graphics drawing. Accumulates paths with
 * their fill/stroke settings, then renders them all at once.
 *
 * Example:
 * @code
 * // In a draw event handler:
 * lv::VectorDsc dsc(layer);
 *
 * // Draw a filled triangle
 * lv::VectorPath triangle;
 * triangle.move_to(100, 50).line_to(150, 150).line_to(50, 150).close();
 *
 * dsc.fill_color(lv::palette::blue())
 *    .fill_opa(LV_OPA_80)
 *    .add_path(triangle);
 *
 * // Draw a stroked circle
 * lv::VectorPath circle_path;
 * circle_path.circle(200, 100, 40);
 *
 * dsc.fill_opa(LV_OPA_TRANSP)  // No fill
 *    .stroke_color(lv::palette::red())
 *    .stroke_width(3.0f)
 *    .add_path(circle_path);
 *
 * dsc.draw();  // Render everything
 * @endcode
 */
class VectorDsc {
    lv_draw_vector_dsc_t* m_dsc = nullptr;

public:
    /// Create a vector descriptor for a layer
    explicit VectorDsc(lv_layer_t* layer) noexcept
        : m_dsc(lv_draw_vector_dsc_create(layer)) {}

    /// Non-copyable
    VectorDsc(const VectorDsc&) = delete;
    VectorDsc& operator=(const VectorDsc&) = delete;

    /// Moveable
    VectorDsc(VectorDsc&& other) noexcept : m_dsc(other.m_dsc) {
        other.m_dsc = nullptr;
    }

    VectorDsc& operator=(VectorDsc&& other) noexcept {
        if (this != &other) {
            if (m_dsc) lv_draw_vector_dsc_delete(m_dsc);
            m_dsc = other.m_dsc;
            other.m_dsc = nullptr;
        }
        return *this;
    }

    /// Destructor
    ~VectorDsc() {
        if (m_dsc) lv_draw_vector_dsc_delete(m_dsc);
    }

    /// Get underlying pointer
    [[nodiscard]] lv_draw_vector_dsc_t* get() noexcept { return m_dsc; }
    [[nodiscard]] const lv_draw_vector_dsc_t* get() const noexcept { return m_dsc; }

    /// Check if valid
    [[nodiscard]] bool valid() const noexcept { return m_dsc != nullptr; }
    explicit operator bool() const noexcept { return valid(); }

    // ==================== Transform ====================

    /// Reset transformation to identity matrix
    VectorDsc& identity() noexcept {
        lv_draw_vector_dsc_identity(m_dsc);
        return *this;
    }

    /// Set custom transformation matrix
    VectorDsc& transform(const lv_matrix_t& matrix) noexcept {
        lv_draw_vector_dsc_set_transform(m_dsc, &matrix);
        return *this;
    }

    /// Scale
    VectorDsc& scale(float sx, float sy) noexcept {
        lv_draw_vector_dsc_scale(m_dsc, sx, sy);
        return *this;
    }

    VectorDsc& scale(float s) noexcept {
        return scale(s, s);
    }

    /// Rotate (degrees)
    VectorDsc& rotate(float degrees) noexcept {
        lv_draw_vector_dsc_rotate(m_dsc, degrees);
        return *this;
    }

    /// Translate
    VectorDsc& translate(float tx, float ty) noexcept {
        lv_draw_vector_dsc_translate(m_dsc, tx, ty);
        return *this;
    }

    /// Skew
    VectorDsc& skew(float sx, float sy) noexcept {
        lv_draw_vector_dsc_skew(m_dsc, sx, sy);
        return *this;
    }

    // ==================== Blend Mode ====================

    VectorDsc& blend_mode(lv_vector_blend_t mode) noexcept {
        lv_draw_vector_dsc_set_blend_mode(m_dsc, mode);
        return *this;
    }

    // ==================== Fill Settings ====================

    /// Set fill color (lv_color_t)
    VectorDsc& fill_color(lv_color_t color) noexcept {
        lv_draw_vector_dsc_set_fill_color(m_dsc, color);
        return *this;
    }

    /// Set fill color (lv_color32_t with alpha)
    VectorDsc& fill_color32(lv_color32_t color) noexcept {
        lv_draw_vector_dsc_set_fill_color32(m_dsc, color);
        return *this;
    }

    /// Set fill opacity
    VectorDsc& fill_opa(lv_opa_t opa) noexcept {
        lv_draw_vector_dsc_set_fill_opa(m_dsc, opa);
        return *this;
    }

    /// Set fill rule (non-zero or even-odd)
    VectorDsc& fill_rule(lv_vector_fill_t rule) noexcept {
        lv_draw_vector_dsc_set_fill_rule(m_dsc, rule);
        return *this;
    }

    /// Set fill coordinate units
    VectorDsc& fill_units(lv_vector_fill_units_t units) noexcept {
        lv_draw_vector_dsc_set_fill_units(m_dsc, units);
        return *this;
    }

    /// Set fill image
    VectorDsc& fill_image(const lv_draw_image_dsc_t& img_dsc) noexcept {
        lv_draw_vector_dsc_set_fill_image(m_dsc, &img_dsc);
        return *this;
    }

    /// Set fill transformation matrix
    VectorDsc& fill_transform(const lv_matrix_t& matrix) noexcept {
        lv_draw_vector_dsc_set_fill_transform(m_dsc, &matrix);
        return *this;
    }

    // ==================== Fill Gradient ====================

    /// Set linear gradient for fill
    VectorDsc& fill_linear_gradient(float x1, float y1, float x2, float y2) noexcept {
        lv_draw_vector_dsc_set_fill_linear_gradient(m_dsc, x1, y1, x2, y2);
        return *this;
    }

    /// Set radial gradient for fill
    VectorDsc& fill_radial_gradient(float cx, float cy, float radius) noexcept {
        lv_draw_vector_dsc_set_fill_radial_gradient(m_dsc, cx, cy, radius);
        return *this;
    }

    /// Set gradient spread mode for fill
    VectorDsc& fill_gradient_spread(lv_vector_gradient_spread_t spread) noexcept {
        lv_draw_vector_dsc_set_fill_gradient_spread(m_dsc, spread);
        return *this;
    }

    /// Set gradient color stops for fill
    VectorDsc& fill_gradient_stops(const lv_grad_stop_t* stops, uint16_t count) noexcept {
        lv_draw_vector_dsc_set_fill_gradient_color_stops(m_dsc, stops, count);
        return *this;
    }

    // ==================== Stroke Settings ====================

    /// Set stroke color (lv_color_t)
    VectorDsc& stroke_color(lv_color_t color) noexcept {
        lv_draw_vector_dsc_set_stroke_color(m_dsc, color);
        return *this;
    }

    /// Set stroke color (lv_color32_t with alpha)
    VectorDsc& stroke_color32(lv_color32_t color) noexcept {
        lv_draw_vector_dsc_set_stroke_color32(m_dsc, color);
        return *this;
    }

    /// Set stroke opacity
    VectorDsc& stroke_opa(lv_opa_t opa) noexcept {
        lv_draw_vector_dsc_set_stroke_opa(m_dsc, opa);
        return *this;
    }

    /// Set stroke width
    VectorDsc& stroke_width(float width) noexcept {
        lv_draw_vector_dsc_set_stroke_width(m_dsc, width);
        return *this;
    }

    /// Set stroke line cap style
    VectorDsc& stroke_cap(lv_vector_stroke_cap_t cap) noexcept {
        lv_draw_vector_dsc_set_stroke_cap(m_dsc, cap);
        return *this;
    }

    /// Set stroke line join style
    VectorDsc& stroke_join(lv_vector_stroke_join_t join) noexcept {
        lv_draw_vector_dsc_set_stroke_join(m_dsc, join);
        return *this;
    }

    /// Set stroke miter limit
    VectorDsc& stroke_miter_limit(uint16_t limit) noexcept {
        lv_draw_vector_dsc_set_stroke_miter_limit(m_dsc, limit);
        return *this;
    }

    /// Set stroke dash pattern
    VectorDsc& stroke_dash(float* pattern, uint16_t count) noexcept {
        lv_draw_vector_dsc_set_stroke_dash(m_dsc, pattern, count);
        return *this;
    }

    /// Set stroke transformation matrix
    VectorDsc& stroke_transform(const lv_matrix_t& matrix) noexcept {
        lv_draw_vector_dsc_set_stroke_transform(m_dsc, &matrix);
        return *this;
    }

    // ==================== Stroke Gradient ====================

    /// Set linear gradient for stroke
    VectorDsc& stroke_linear_gradient(float x1, float y1, float x2, float y2) noexcept {
        lv_draw_vector_dsc_set_stroke_linear_gradient(m_dsc, x1, y1, x2, y2);
        return *this;
    }

    /// Set radial gradient for stroke
    VectorDsc& stroke_radial_gradient(float cx, float cy, float radius) noexcept {
        lv_draw_vector_dsc_set_stroke_radial_gradient(m_dsc, cx, cy, radius);
        return *this;
    }

    /// Set gradient spread mode for stroke
    VectorDsc& stroke_gradient_spread(lv_vector_gradient_spread_t spread) noexcept {
        lv_draw_vector_dsc_set_stroke_gradient_spread(m_dsc, spread);
        return *this;
    }

    /// Set gradient color stops for stroke
    VectorDsc& stroke_gradient_stops(const lv_grad_stop_t* stops, uint16_t count) noexcept {
        lv_draw_vector_dsc_set_stroke_gradient_color_stops(m_dsc, stops, count);
        return *this;
    }

    // ==================== Path Operations ====================

    /// Add a path to the draw list (uses current fill/stroke settings)
    VectorDsc& add_path(const VectorPath& path) noexcept {
        lv_draw_vector_dsc_add_path(m_dsc, path.get());
        return *this;
    }

    /// Clear a rectangular area with current fill color
    VectorDsc& clear_area(const lv_area_t& rect) noexcept {
        lv_draw_vector_dsc_clear_area(m_dsc, &rect);
        return *this;
    }

    // ==================== Rendering ====================

    /// Draw all accumulated paths
    void draw() noexcept {
        lv_draw_vector(m_dsc);
    }
};

// ==================== Matrix Helpers ====================

namespace matrix {

/// Transform a point using a matrix
inline void transform_point(const lv_matrix_t& matrix, FPoint& point) noexcept {
    lv_matrix_transform_point(&matrix, &point);
}

/// Transform a path using a matrix
inline void transform_path(const lv_matrix_t& matrix, VectorPath& path) noexcept {
    lv_matrix_transform_path(&matrix, path.get());
}

} // namespace matrix

// ==================== Draw Task Utility ====================

namespace draw {

/**
 * @brief Get vector descriptor from a draw task
 *
 * @param task Draw task
 * @return Vector descriptor or nullptr if not a vector task
 */
inline lv_draw_vector_dsc_t* get_vector_dsc(lv_draw_task_t* task) noexcept {
    return lv_draw_task_get_vector_dsc(task);
}

} // namespace draw

} // namespace lv

#endif // LV_USE_VECTOR_GRAPHIC
