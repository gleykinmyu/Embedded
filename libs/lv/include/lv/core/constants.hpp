#pragma once

/**
 * @file constants.hpp
 * @brief C++ idiomatic constants for LVGL
 *
 * Provides namespaced constants as an alternative to LVGL's C-style macros.
 * Uses 'k' prefix to avoid conflicts with class names.
 *
 * Example:
 *   obj.align(lv::kAlign::center);
 *   obj.add_style(style, lv::kPart::main | lv::kState::pressed);
 */

#include <lvgl.h>
#include "version.hpp"

namespace lv {

// ============================================================================
// Alignment
// ============================================================================

namespace kAlign {
    constexpr auto default_      = LV_ALIGN_DEFAULT;
    constexpr auto top_left      = LV_ALIGN_TOP_LEFT;
    constexpr auto top_mid       = LV_ALIGN_TOP_MID;
    constexpr auto top_right     = LV_ALIGN_TOP_RIGHT;
    constexpr auto bottom_left   = LV_ALIGN_BOTTOM_LEFT;
    constexpr auto bottom_mid    = LV_ALIGN_BOTTOM_MID;
    constexpr auto bottom_right  = LV_ALIGN_BOTTOM_RIGHT;
    constexpr auto left_mid      = LV_ALIGN_LEFT_MID;
    constexpr auto right_mid     = LV_ALIGN_RIGHT_MID;
    constexpr auto center        = LV_ALIGN_CENTER;

    // Out-of-parent alignments
    constexpr auto out_top_left     = LV_ALIGN_OUT_TOP_LEFT;
    constexpr auto out_top_mid      = LV_ALIGN_OUT_TOP_MID;
    constexpr auto out_top_right    = LV_ALIGN_OUT_TOP_RIGHT;
    constexpr auto out_bottom_left  = LV_ALIGN_OUT_BOTTOM_LEFT;
    constexpr auto out_bottom_mid   = LV_ALIGN_OUT_BOTTOM_MID;
    constexpr auto out_bottom_right = LV_ALIGN_OUT_BOTTOM_RIGHT;
    constexpr auto out_left_top     = LV_ALIGN_OUT_LEFT_TOP;
    constexpr auto out_left_mid     = LV_ALIGN_OUT_LEFT_MID;
    constexpr auto out_left_bottom  = LV_ALIGN_OUT_LEFT_BOTTOM;
    constexpr auto out_right_top    = LV_ALIGN_OUT_RIGHT_TOP;
    constexpr auto out_right_mid    = LV_ALIGN_OUT_RIGHT_MID;
    constexpr auto out_right_bottom = LV_ALIGN_OUT_RIGHT_BOTTOM;
}

// ============================================================================
// Object Parts
// ============================================================================

namespace kPart {
    constexpr lv_part_t main       = LV_PART_MAIN;
    constexpr lv_part_t scrollbar  = LV_PART_SCROLLBAR;
    constexpr lv_part_t indicator  = LV_PART_INDICATOR;
    constexpr lv_part_t knob       = LV_PART_KNOB;
    constexpr lv_part_t selected   = LV_PART_SELECTED;
    constexpr lv_part_t items      = LV_PART_ITEMS;
    constexpr lv_part_t cursor     = LV_PART_CURSOR;
    constexpr lv_part_t custom1    = LV_PART_CUSTOM_FIRST;
    constexpr lv_part_t any        = LV_PART_ANY;
}

// ============================================================================
// Object States
// ============================================================================

namespace kState {
    constexpr lv_state_t default_      = LV_STATE_DEFAULT;
#if LV_VERSION_AT_LEAST(9, 5, 0)
    constexpr lv_state_t alt           = LV_STATE_ALT;           ///< @since LVGL 9.5
#endif
    constexpr lv_state_t checked       = LV_STATE_CHECKED;
    constexpr lv_state_t focused       = LV_STATE_FOCUSED;
    constexpr lv_state_t focus_key     = LV_STATE_FOCUS_KEY;
    constexpr lv_state_t edited        = LV_STATE_EDITED;
    constexpr lv_state_t hovered       = LV_STATE_HOVERED;
    constexpr lv_state_t pressed       = LV_STATE_PRESSED;
    constexpr lv_state_t scrolled      = LV_STATE_SCROLLED;
    constexpr lv_state_t disabled      = LV_STATE_DISABLED;
    constexpr lv_state_t user1         = LV_STATE_USER_1;
    constexpr lv_state_t user2         = LV_STATE_USER_2;
    constexpr lv_state_t user3         = LV_STATE_USER_3;
    constexpr lv_state_t user4         = LV_STATE_USER_4;
    constexpr lv_state_t any           = LV_STATE_ANY;
}

// ============================================================================
// Object Flags
// ============================================================================

namespace kFlag {
    constexpr auto hidden              = LV_OBJ_FLAG_HIDDEN;
    constexpr auto clickable           = LV_OBJ_FLAG_CLICKABLE;
    constexpr auto click_focusable     = LV_OBJ_FLAG_CLICK_FOCUSABLE;
    constexpr auto checkable           = LV_OBJ_FLAG_CHECKABLE;
    constexpr auto scrollable          = LV_OBJ_FLAG_SCROLLABLE;
    constexpr auto scroll_elastic      = LV_OBJ_FLAG_SCROLL_ELASTIC;
    constexpr auto scroll_momentum     = LV_OBJ_FLAG_SCROLL_MOMENTUM;
    constexpr auto scroll_one          = LV_OBJ_FLAG_SCROLL_ONE;
    constexpr auto scroll_chain_hor    = LV_OBJ_FLAG_SCROLL_CHAIN_HOR;
    constexpr auto scroll_chain_ver    = LV_OBJ_FLAG_SCROLL_CHAIN_VER;
    constexpr auto scroll_chain        = LV_OBJ_FLAG_SCROLL_CHAIN;
    constexpr auto scroll_on_focus     = LV_OBJ_FLAG_SCROLL_ON_FOCUS;
    constexpr auto scroll_with_arrow   = LV_OBJ_FLAG_SCROLL_WITH_ARROW;
    constexpr auto snappable           = LV_OBJ_FLAG_SNAPPABLE;
    constexpr auto press_lock          = LV_OBJ_FLAG_PRESS_LOCK;
    constexpr auto event_bubble        = LV_OBJ_FLAG_EVENT_BUBBLE;
    constexpr auto gesture_bubble      = LV_OBJ_FLAG_GESTURE_BUBBLE;
    constexpr auto adv_hittest         = LV_OBJ_FLAG_ADV_HITTEST;
    constexpr auto ignore_layout       = LV_OBJ_FLAG_IGNORE_LAYOUT;
    constexpr auto floating            = LV_OBJ_FLAG_FLOATING;
    constexpr auto overflow_visible    = LV_OBJ_FLAG_OVERFLOW_VISIBLE;
#if LV_USE_FLEX
    constexpr auto flex_in_new_track   = LV_OBJ_FLAG_FLEX_IN_NEW_TRACK;
#endif
    constexpr auto user_1              = LV_OBJ_FLAG_USER_1;
    constexpr auto user_2              = LV_OBJ_FLAG_USER_2;
    constexpr auto user_3              = LV_OBJ_FLAG_USER_3;
    constexpr auto user_4              = LV_OBJ_FLAG_USER_4;
}

// ============================================================================
// Opacity
// ============================================================================

namespace kOpa {
    constexpr lv_opa_t transp  = LV_OPA_TRANSP;
    constexpr lv_opa_t _0      = LV_OPA_0;
    constexpr lv_opa_t _10     = LV_OPA_10;
    constexpr lv_opa_t _20     = LV_OPA_20;
    constexpr lv_opa_t _30     = LV_OPA_30;
    constexpr lv_opa_t _40     = LV_OPA_40;
    constexpr lv_opa_t _50     = LV_OPA_50;
    constexpr lv_opa_t _60     = LV_OPA_60;
    constexpr lv_opa_t _70     = LV_OPA_70;
    constexpr lv_opa_t _80     = LV_OPA_80;
    constexpr lv_opa_t _90     = LV_OPA_90;
    constexpr lv_opa_t _100    = LV_OPA_100;
    constexpr lv_opa_t cover   = LV_OPA_COVER;
}

// ============================================================================
// Direction
// ============================================================================

namespace kDir {
    constexpr lv_dir_t none   = LV_DIR_NONE;
    constexpr lv_dir_t left   = LV_DIR_LEFT;
    constexpr lv_dir_t right  = LV_DIR_RIGHT;
    constexpr lv_dir_t top    = LV_DIR_TOP;
    constexpr lv_dir_t bottom = LV_DIR_BOTTOM;
    constexpr lv_dir_t hor    = LV_DIR_HOR;
    constexpr lv_dir_t ver    = LV_DIR_VER;
    constexpr lv_dir_t all    = LV_DIR_ALL;
}

// ============================================================================
// Flex Layout
// ============================================================================

#if LV_USE_FLEX
namespace kFlexFlow {
    constexpr auto row            = LV_FLEX_FLOW_ROW;
    constexpr auto column         = LV_FLEX_FLOW_COLUMN;
    constexpr auto row_wrap       = LV_FLEX_FLOW_ROW_WRAP;
    constexpr auto column_wrap    = LV_FLEX_FLOW_COLUMN_WRAP;
    constexpr auto row_reverse    = LV_FLEX_FLOW_ROW_REVERSE;
    constexpr auto column_reverse = LV_FLEX_FLOW_COLUMN_REVERSE;
}

namespace kFlexAlign {
    constexpr auto start         = LV_FLEX_ALIGN_START;
    constexpr auto end           = LV_FLEX_ALIGN_END;
    constexpr auto center        = LV_FLEX_ALIGN_CENTER;
    constexpr auto space_evenly  = LV_FLEX_ALIGN_SPACE_EVENLY;
    constexpr auto space_around  = LV_FLEX_ALIGN_SPACE_AROUND;
    constexpr auto space_between = LV_FLEX_ALIGN_SPACE_BETWEEN;
}
#endif

// ============================================================================
// Grid Layout
// ============================================================================

#if LV_USE_GRID
namespace kGridAlign {
    constexpr auto start         = LV_GRID_ALIGN_START;
    constexpr auto center        = LV_GRID_ALIGN_CENTER;
    constexpr auto end           = LV_GRID_ALIGN_END;
    constexpr auto stretch       = LV_GRID_ALIGN_STRETCH;
    constexpr auto space_evenly  = LV_GRID_ALIGN_SPACE_EVENLY;
    constexpr auto space_around  = LV_GRID_ALIGN_SPACE_AROUND;
    constexpr auto space_between = LV_GRID_ALIGN_SPACE_BETWEEN;
}
#endif

// ============================================================================
// Text
// ============================================================================

namespace kTextAlign {
    constexpr auto auto_   = LV_TEXT_ALIGN_AUTO;
    constexpr auto left    = LV_TEXT_ALIGN_LEFT;
    constexpr auto center  = LV_TEXT_ALIGN_CENTER;
    constexpr auto right   = LV_TEXT_ALIGN_RIGHT;
}

namespace kTextDecor {
    constexpr auto none          = LV_TEXT_DECOR_NONE;
    constexpr auto underline     = LV_TEXT_DECOR_UNDERLINE;
    constexpr auto strikethrough = LV_TEXT_DECOR_STRIKETHROUGH;
}

// ============================================================================
// Label
// ============================================================================

#if LV_USE_LABEL
namespace kLabelLongMode {
    constexpr auto wrap            = LV_LABEL_LONG_WRAP;
    constexpr auto dot             = LV_LABEL_LONG_DOT;
    constexpr auto scroll          = LV_LABEL_LONG_SCROLL;
    constexpr auto scroll_circular = LV_LABEL_LONG_SCROLL_CIRCULAR;
    constexpr auto clip            = LV_LABEL_LONG_CLIP;
}
#endif

// ============================================================================
// Arc
// ============================================================================

#if LV_USE_ARC
namespace kArcMode {
    constexpr auto normal      = LV_ARC_MODE_NORMAL;
    constexpr auto symmetrical = LV_ARC_MODE_SYMMETRICAL;
    constexpr auto reverse     = LV_ARC_MODE_REVERSE;
}
#endif

// ============================================================================
// Bar
// ============================================================================

#if LV_USE_BAR
namespace kBarMode {
    constexpr auto normal      = LV_BAR_MODE_NORMAL;
    constexpr auto symmetrical = LV_BAR_MODE_SYMMETRICAL;
    constexpr auto range       = LV_BAR_MODE_RANGE;
}
#endif

// ============================================================================
// Slider
// ============================================================================

#if LV_USE_SLIDER
namespace kSliderMode {
    constexpr auto normal      = LV_SLIDER_MODE_NORMAL;
    constexpr auto symmetrical = LV_SLIDER_MODE_SYMMETRICAL;
    constexpr auto range       = LV_SLIDER_MODE_RANGE;
}
#endif

// ============================================================================
// Chart
// ============================================================================

#if LV_USE_CHART
namespace kChartType {
    constexpr auto none    = LV_CHART_TYPE_NONE;
    constexpr auto line    = LV_CHART_TYPE_LINE;
    constexpr auto bar     = LV_CHART_TYPE_BAR;
    constexpr auto scatter = LV_CHART_TYPE_SCATTER;
#if LV_VERSION_AT_LEAST(9, 5, 0)
    constexpr auto curve   = LV_CHART_TYPE_CURVE;    ///< @since LVGL 9.5
#endif
}

namespace kChartAxis {
    constexpr auto primary_y   = LV_CHART_AXIS_PRIMARY_Y;
    constexpr auto secondary_y = LV_CHART_AXIS_SECONDARY_Y;
    constexpr auto primary_x   = LV_CHART_AXIS_PRIMARY_X;
    constexpr auto secondary_x = LV_CHART_AXIS_SECONDARY_X;
}

namespace kChartUpdateMode {
    constexpr auto shift    = LV_CHART_UPDATE_MODE_SHIFT;
    constexpr auto circular = LV_CHART_UPDATE_MODE_CIRCULAR;
}
#endif

// ============================================================================
// Gradient
// ============================================================================

namespace kGradDir {
    constexpr auto none = LV_GRAD_DIR_NONE;
    constexpr auto ver  = LV_GRAD_DIR_VER;
    constexpr auto hor  = LV_GRAD_DIR_HOR;
}

// ============================================================================
// Border Side
// ============================================================================

namespace kBorderSide {
    constexpr auto none   = LV_BORDER_SIDE_NONE;
    constexpr auto bottom = LV_BORDER_SIDE_BOTTOM;
    constexpr auto top    = LV_BORDER_SIDE_TOP;
    constexpr auto left   = LV_BORDER_SIDE_LEFT;
    constexpr auto right  = LV_BORDER_SIDE_RIGHT;
    constexpr auto full   = LV_BORDER_SIDE_FULL;
}

// ============================================================================
// Scrollbar Mode
// ============================================================================

namespace kScrollbarMode {
    constexpr auto off    = LV_SCROLLBAR_MODE_OFF;
    constexpr auto on     = LV_SCROLLBAR_MODE_ON;
    constexpr auto active = LV_SCROLLBAR_MODE_ACTIVE;
    constexpr auto auto_  = LV_SCROLLBAR_MODE_AUTO;
}

// ============================================================================
// Scroll Snap
// ============================================================================

namespace kScrollSnap {
    constexpr auto none   = LV_SCROLL_SNAP_NONE;
    constexpr auto start  = LV_SCROLL_SNAP_START;
    constexpr auto end    = LV_SCROLL_SNAP_END;
    constexpr auto center = LV_SCROLL_SNAP_CENTER;
}

// ============================================================================
// Events
// ============================================================================

/**
 * @brief Event codes for LVGL event handling
 *
 * Events are grouped by target type:
 * - **Widget events**: Fire on lv_obj_t widgets (use with btn.on(...), label.on(...), etc.)
 * - **Display events**: Fire on lv_display_t objects (use with lv_display_add_event_cb())
 *
 * Most events in this namespace are widget events. Display-specific events are
 * clearly marked in the "Display/render events" section below.
 *
 * ## EventMixin Conveniences
 *
 * Common widget events have shorthand helpers in EventMixin (on_click, on_scroll, etc.).
 * For events without shortcuts, use: btn.on(lv::kEvent::rotary, handler)
 *
 * ## Display Events
 *
 * Events marked "Display:" must be registered via lv_display_add_event_cb(), NOT widget.on().
 * Registering them on a widget will compile but the callback will never fire.
 */
namespace kEvent {
    // -------------------- Widget: Input device events --------------------
    // These fire on widgets in response to user input
    constexpr auto pressed             = LV_EVENT_PRESSED;
    constexpr auto pressing            = LV_EVENT_PRESSING;
    constexpr auto press_lost          = LV_EVENT_PRESS_LOST;
    constexpr auto short_clicked       = LV_EVENT_SHORT_CLICKED;
    constexpr auto single_clicked      = LV_EVENT_SINGLE_CLICKED;
    constexpr auto double_clicked      = LV_EVENT_DOUBLE_CLICKED;
    constexpr auto triple_clicked      = LV_EVENT_TRIPLE_CLICKED;
    constexpr auto long_pressed        = LV_EVENT_LONG_PRESSED;
    constexpr auto long_pressed_repeat = LV_EVENT_LONG_PRESSED_REPEAT;
    constexpr auto clicked             = LV_EVENT_CLICKED;
    constexpr auto released            = LV_EVENT_RELEASED;
    constexpr auto scroll_begin        = LV_EVENT_SCROLL_BEGIN;
    constexpr auto scroll_throw_begin  = LV_EVENT_SCROLL_THROW_BEGIN;
    constexpr auto scroll_end          = LV_EVENT_SCROLL_END;
    constexpr auto scroll              = LV_EVENT_SCROLL;
    constexpr auto gesture             = LV_EVENT_GESTURE;
    constexpr auto key                 = LV_EVENT_KEY;
    constexpr auto rotary              = LV_EVENT_ROTARY;
    constexpr auto focused             = LV_EVENT_FOCUSED;
    constexpr auto defocused           = LV_EVENT_DEFOCUSED;
    constexpr auto leave               = LV_EVENT_LEAVE;
    constexpr auto hit_test            = LV_EVENT_HIT_TEST;
    constexpr auto indev_reset         = LV_EVENT_INDEV_RESET;
    constexpr auto hover_over          = LV_EVENT_HOVER_OVER;
    constexpr auto hover_leave         = LV_EVENT_HOVER_LEAVE;

    // -------------------- Widget: Drawing events --------------------
    // These fire on widgets during rendering
    constexpr auto cover_check         = LV_EVENT_COVER_CHECK;
    constexpr auto refr_ext_draw_size  = LV_EVENT_REFR_EXT_DRAW_SIZE;
    constexpr auto draw_main_begin     = LV_EVENT_DRAW_MAIN_BEGIN;
    constexpr auto draw_main           = LV_EVENT_DRAW_MAIN;
    constexpr auto draw_main_end       = LV_EVENT_DRAW_MAIN_END;
    constexpr auto draw_post_begin     = LV_EVENT_DRAW_POST_BEGIN;
    constexpr auto draw_post           = LV_EVENT_DRAW_POST;
    constexpr auto draw_post_end       = LV_EVENT_DRAW_POST_END;
    constexpr auto draw_task_added     = LV_EVENT_DRAW_TASK_ADDED;  ///< Requires LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS

    // -------------------- Widget: Value/state events --------------------
    // These fire on widgets when their value or state changes
    constexpr auto value_changed       = LV_EVENT_VALUE_CHANGED;
    constexpr auto insert              = LV_EVENT_INSERT;
    constexpr auto refresh             = LV_EVENT_REFRESH;
    constexpr auto ready               = LV_EVENT_READY;
    constexpr auto cancel              = LV_EVENT_CANCEL;
#ifdef LV_EVENT_STATE_CHANGED
    constexpr auto state_changed       = LV_EVENT_STATE_CHANGED;
#endif

    // -------------------- Widget: Lifecycle events --------------------
    // These fire on widgets during creation, deletion, and structural changes
    constexpr auto create              = LV_EVENT_CREATE;
    constexpr auto delete_             = LV_EVENT_DELETE;
    constexpr auto child_changed       = LV_EVENT_CHILD_CHANGED;
    constexpr auto child_created       = LV_EVENT_CHILD_CREATED;
    constexpr auto child_deleted       = LV_EVENT_CHILD_DELETED;
    constexpr auto size_changed        = LV_EVENT_SIZE_CHANGED;
    constexpr auto style_changed       = LV_EVENT_STYLE_CHANGED;
    constexpr auto layout_changed      = LV_EVENT_LAYOUT_CHANGED;
    constexpr auto get_self_size       = LV_EVENT_GET_SELF_SIZE;

    // -------------------- Screen events --------------------
    // These fire on screen objects during screen transitions
    constexpr auto screen_unload_start = LV_EVENT_SCREEN_UNLOAD_START;
    constexpr auto screen_load_start   = LV_EVENT_SCREEN_LOAD_START;
    constexpr auto screen_loaded       = LV_EVENT_SCREEN_LOADED;
    constexpr auto screen_unloaded     = LV_EVENT_SCREEN_UNLOADED;

    // -------------------- Display events (lv_display_t) --------------------
    // NOTE: These fire on DISPLAY objects, not widgets!
    // Register with lv_display_add_event_cb(), not widget.on()
    constexpr auto invalidate_area         = LV_EVENT_INVALIDATE_AREA;   ///< Display: area invalidated for redraw
    constexpr auto resolution_changed      = LV_EVENT_RESOLUTION_CHANGED;///< Display: resolution or rotation changed
    constexpr auto color_format_changed    = LV_EVENT_COLOR_FORMAT_CHANGED; ///< Display: color format changed
    constexpr auto refr_request            = LV_EVENT_REFR_REQUEST;      ///< Display: redraw requested
    constexpr auto refr_start              = LV_EVENT_REFR_START;        ///< Display: refresh cycle starting
    constexpr auto refr_ready              = LV_EVENT_REFR_READY;        ///< Display: refresh cycle complete
    constexpr auto render_start            = LV_EVENT_RENDER_START;      ///< Display: rendering starting
    constexpr auto render_ready            = LV_EVENT_RENDER_READY;      ///< Display: rendering complete
    constexpr auto flush_start             = LV_EVENT_FLUSH_START;       ///< Display: flush callback starting
    constexpr auto flush_finish            = LV_EVENT_FLUSH_FINISH;      ///< Display: flush callback complete
    constexpr auto flush_wait_start        = LV_EVENT_FLUSH_WAIT_START;  ///< Display: flush wait starting
    constexpr auto flush_wait_finish       = LV_EVENT_FLUSH_WAIT_FINISH; ///< Display: flush wait complete
#ifdef LV_EVENT_UPDATE_LAYOUT_COMPLETED
    constexpr auto update_layout_completed = LV_EVENT_UPDATE_LAYOUT_COMPLETED; ///< Display: layout update complete
#endif
    constexpr auto vsync                   = LV_EVENT_VSYNC;             ///< Display: vsync signal received
    constexpr auto vsync_request           = LV_EVENT_VSYNC_REQUEST;     ///< Display: vsync requested

    // -------------------- Special --------------------
    constexpr auto all                 = LV_EVENT_ALL;        ///< Match all events
    constexpr auto preprocess          = LV_EVENT_PREPROCESS; ///< Flag: process before class default handler
}

// ============================================================================
// Keyboard Modes
// ============================================================================

#if LV_USE_KEYBOARD
namespace kKeyboardMode {
    constexpr auto text_lower   = LV_KEYBOARD_MODE_TEXT_LOWER;
    constexpr auto text_upper   = LV_KEYBOARD_MODE_TEXT_UPPER;
    constexpr auto special      = LV_KEYBOARD_MODE_SPECIAL;
    constexpr auto number       = LV_KEYBOARD_MODE_NUMBER;
    constexpr auto user1        = LV_KEYBOARD_MODE_USER_1;
    constexpr auto user2        = LV_KEYBOARD_MODE_USER_2;
    constexpr auto user3        = LV_KEYBOARD_MODE_USER_3;
    constexpr auto user4        = LV_KEYBOARD_MODE_USER_4;
}
#endif

// ============================================================================
// Blend Mode
// ============================================================================

namespace kBlendMode {
    constexpr auto normal       = LV_BLEND_MODE_NORMAL;
    constexpr auto additive     = LV_BLEND_MODE_ADDITIVE;
    constexpr auto subtractive  = LV_BLEND_MODE_SUBTRACTIVE;
    constexpr auto multiply     = LV_BLEND_MODE_MULTIPLY;
    constexpr auto difference   = LV_BLEND_MODE_DIFFERENCE;
}

// ============================================================================
// Layout
// ============================================================================

namespace kLayout {
    constexpr auto none = LV_LAYOUT_NONE;
#if LV_USE_FLEX
    constexpr auto flex = LV_LAYOUT_FLEX;
#endif
#if LV_USE_GRID
    constexpr auto grid = LV_LAYOUT_GRID;
#endif
}

// ============================================================================
// Animation Enable
// ============================================================================

namespace kAnim {
    constexpr auto on  = LV_ANIM_ON;
    constexpr auto off = LV_ANIM_OFF;
}

// ============================================================================
// Size helpers
// ============================================================================

namespace kSize {
    constexpr auto content = LV_SIZE_CONTENT;
}

namespace kRadius {
    constexpr int32_t circle = LV_RADIUS_CIRCLE;
}

// ============================================================================
// Color Format
// ============================================================================

namespace kColorFormat {
    // Unknown/special
    constexpr auto unknown            = LV_COLOR_FORMAT_UNKNOWN;
    constexpr auto raw                = LV_COLOR_FORMAT_RAW;
    constexpr auto raw_alpha          = LV_COLOR_FORMAT_RAW_ALPHA;

    // Grayscale / Indexed
    constexpr auto L8                 = LV_COLOR_FORMAT_L8;
    constexpr auto I1                 = LV_COLOR_FORMAT_I1;
    constexpr auto I2                 = LV_COLOR_FORMAT_I2;
    constexpr auto I4                 = LV_COLOR_FORMAT_I4;
    constexpr auto I8                 = LV_COLOR_FORMAT_I8;

    // Alpha only
    constexpr auto A1                 = LV_COLOR_FORMAT_A1;
    constexpr auto A2                 = LV_COLOR_FORMAT_A2;
    constexpr auto A4                 = LV_COLOR_FORMAT_A4;
    constexpr auto A8                 = LV_COLOR_FORMAT_A8;
    constexpr auto AL88               = LV_COLOR_FORMAT_AL88;

    // RGB formats
    constexpr auto RGB565             = LV_COLOR_FORMAT_RGB565;
    constexpr auto RGB565_swapped     = LV_COLOR_FORMAT_RGB565_SWAPPED;
    constexpr auto RGB565A8           = LV_COLOR_FORMAT_RGB565A8;
    constexpr auto RGB888             = LV_COLOR_FORMAT_RGB888;

    // ARGB formats (most common)
    constexpr auto ARGB8565           = LV_COLOR_FORMAT_ARGB8565;
    constexpr auto ARGB8888           = LV_COLOR_FORMAT_ARGB8888;
    constexpr auto XRGB8888           = LV_COLOR_FORMAT_XRGB8888;
    constexpr auto ARGB8888_premult   = LV_COLOR_FORMAT_ARGB8888_PREMULTIPLIED;
    constexpr auto ARGB1555           = LV_COLOR_FORMAT_ARGB1555;
    constexpr auto ARGB4444           = LV_COLOR_FORMAT_ARGB4444;
    constexpr auto ARGB2222           = LV_COLOR_FORMAT_ARGB2222;

    // YUV formats
    constexpr auto I420               = LV_COLOR_FORMAT_I420;
    constexpr auto I422               = LV_COLOR_FORMAT_I422;
    constexpr auto I444               = LV_COLOR_FORMAT_I444;
    constexpr auto I400               = LV_COLOR_FORMAT_I400;
    constexpr auto NV21               = LV_COLOR_FORMAT_NV21;
    constexpr auto NV12               = LV_COLOR_FORMAT_NV12;
    constexpr auto YUY2               = LV_COLOR_FORMAT_YUY2;
    constexpr auto UYVY               = LV_COLOR_FORMAT_UYVY;

    // Native (depends on LV_COLOR_DEPTH)
    constexpr auto native             = LV_COLOR_FORMAT_NATIVE;
    constexpr auto native_alpha       = LV_COLOR_FORMAT_NATIVE_WITH_ALPHA;
}

// ============================================================================
// Display Rotation
// ============================================================================

namespace kDisplayRotation {
    constexpr auto none               = LV_DISPLAY_ROTATION_0;
    constexpr auto r90                = LV_DISPLAY_ROTATION_90;
    constexpr auto r180               = LV_DISPLAY_ROTATION_180;
    constexpr auto r270               = LV_DISPLAY_ROTATION_270;
}

// ============================================================================
// Coordinate helpers
// ============================================================================

/// Create percentage coordinate
constexpr int32_t pct(int32_t v) { return LV_PCT(v); }

} // namespace lv
