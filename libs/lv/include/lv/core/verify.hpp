#pragma once

/**
 * @file verify.hpp
 * @brief Compile-time verification of zero-cost abstractions
 *
 * This header contains static_asserts to verify that all widget wrappers
 * remain pointer-sized. Include this header in a compilation unit to
 * catch regressions when adding fields to widgets.
 *
 * Usage:
 *   // In a test file or main compilation unit:
 *   #include <lv/core/verify.hpp>
 *
 * If any widget grows beyond pointer size, compilation will fail with
 * a clear error message indicating which widget violated the constraint.
 */

#include "../lv.hpp"

namespace lv::verify {

// ==================== Core Types ====================

static_assert(sizeof(ObjectView) == sizeof(void*),
    "ObjectView must be pointer-sized");

static_assert(sizeof(Object) == sizeof(void*),
    "Object must be pointer-sized");

static_assert(sizeof(Event) == sizeof(void*),
    "Event must be pointer-sized");

static_assert(sizeof(Style) == sizeof(lv_style_t),
    "Style must be lv_style_t-sized");

static_assert(sizeof(Group) == sizeof(void*),
    "Group must be pointer-sized");

static_assert(sizeof(AnimTimeline) == sizeof(void*),
    "AnimTimeline must be pointer-sized");

// ==================== Basic Widgets ====================

static_assert(sizeof(Box) == sizeof(void*),
    "Box must be pointer-sized");

#if LV_USE_LABEL
static_assert(sizeof(Label) == sizeof(void*),
    "Label must be pointer-sized");
#endif

#if LV_USE_BUTTON
static_assert(sizeof(Button) == sizeof(void*),
    "Button must be pointer-sized");
#endif

#if LV_USE_IMAGE
static_assert(sizeof(Image) == sizeof(void*),
    "Image must be pointer-sized");
#endif

#if LV_USE_LINE
static_assert(sizeof(Line) == sizeof(void*),
    "Line must be pointer-sized");
#endif

#if LV_USE_LED
static_assert(sizeof(Led) == sizeof(void*),
    "Led must be pointer-sized");
#endif

// ==================== Input Widgets ====================

#if LV_USE_SWITCH
static_assert(sizeof(Switch) == sizeof(void*),
    "Switch must be pointer-sized");
#endif

#if LV_USE_SLIDER
static_assert(sizeof(Slider) == sizeof(void*),
    "Slider must be pointer-sized");
#endif

#if LV_USE_DROPDOWN
static_assert(sizeof(Dropdown) == sizeof(void*),
    "Dropdown must be pointer-sized");
#endif

#if LV_USE_CHECKBOX
static_assert(sizeof(Checkbox) == sizeof(void*),
    "Checkbox must be pointer-sized");
#endif

#if LV_USE_ROLLER
static_assert(sizeof(Roller) == sizeof(void*),
    "Roller must be pointer-sized");
#endif

#if LV_USE_TEXTAREA
static_assert(sizeof(Textarea) == sizeof(void*),
    "Textarea must be pointer-sized");
#endif

#if LV_USE_SPINBOX
static_assert(sizeof(Spinbox) == sizeof(void*),
    "Spinbox must be pointer-sized");
#endif

#if LV_USE_KEYBOARD
static_assert(sizeof(Keyboard) == sizeof(void*),
    "Keyboard must be pointer-sized");
#endif

#if LV_USE_BUTTONMATRIX
static_assert(sizeof(ButtonMatrix) == sizeof(void*),
    "ButtonMatrix must be pointer-sized");
#endif

// ==================== Display Widgets ====================

#if LV_USE_ARC
static_assert(sizeof(Arc) == sizeof(void*),
    "Arc must be pointer-sized");
#endif

#if LV_USE_BAR
static_assert(sizeof(Bar) == sizeof(void*),
    "Bar must be pointer-sized");
#endif

#if LV_USE_SPINNER
static_assert(sizeof(Spinner) == sizeof(void*),
    "Spinner must be pointer-sized");
#endif

#if LV_USE_CHART
static_assert(sizeof(Chart) == sizeof(void*),
    "Chart must be pointer-sized");
#endif

#if LV_USE_SCALE
static_assert(sizeof(Scale) == sizeof(void*),
    "Scale must be pointer-sized");
#endif

#if LV_USE_TABLE
static_assert(sizeof(Table) == sizeof(void*),
    "Table must be pointer-sized");
#endif

#if LV_USE_CANVAS
static_assert(sizeof(Canvas) == sizeof(void*),
    "Canvas must be pointer-sized");
#endif

#if LV_USE_ANIMIMG
static_assert(sizeof(AnimImage) == sizeof(void*),
    "AnimImage must be pointer-sized");
#endif

#if LV_USE_SPAN
static_assert(sizeof(Spangroup) == sizeof(void*),
    "Spangroup must be pointer-sized");
#endif

// ==================== Container Widgets ====================

#if LV_USE_LIST
static_assert(sizeof(List) == sizeof(void*),
    "List must be pointer-sized");
#endif

#if LV_USE_MENU
static_assert(sizeof(Menu) == sizeof(void*),
    "Menu must be pointer-sized");
#endif

#if LV_USE_TABVIEW
static_assert(sizeof(Tabview) == sizeof(void*),
    "Tabview must be pointer-sized");
#endif

#if LV_USE_TILEVIEW
static_assert(sizeof(Tileview) == sizeof(void*),
    "Tileview must be pointer-sized");
#endif

#if LV_USE_WIN
static_assert(sizeof(Window) == sizeof(void*),
    "Window must be pointer-sized");
#endif

#if LV_USE_MSGBOX
static_assert(sizeof(Msgbox) == sizeof(void*),
    "Msgbox must be pointer-sized");
#endif

#if LV_USE_CALENDAR
static_assert(sizeof(Calendar) == sizeof(void*),
    "Calendar must be pointer-sized");
#endif

#if LV_USE_IMAGEBUTTON
static_assert(sizeof(ImageButton) == sizeof(void*),
    "ImageButton must be pointer-sized");
#endif

// ==================== Layout ====================

#if LV_USE_GRID
static_assert(sizeof(Grid) == sizeof(void*),
    "Grid must be pointer-sized");
#endif

#if LV_USE_FLEX
static_assert(sizeof(Flex) == sizeof(void*),
    "Flex must be pointer-sized");
#endif

// ==================== Optional Widgets ====================

#if LV_USE_LOTTIE
static_assert(sizeof(Lottie) == sizeof(void*),
    "Lottie must be pointer-sized");
#endif

#if LV_USE_QRCODE
static_assert(sizeof(QRCode) == sizeof(void*),
    "QRCode must be pointer-sized");
#endif

#if LV_USE_BARCODE
static_assert(sizeof(Barcode) == sizeof(void*),
    "Barcode must be pointer-sized");
#endif

#if LV_USE_GIF
static_assert(sizeof(Gif) == sizeof(void*),
    "Gif must be pointer-sized");
#endif

#if LV_USE_GLTF
static_assert(sizeof(GLTF) == sizeof(void*),
    "GLTF must be pointer-sized");
#endif

#if LV_USE_ARCLABEL
static_assert(sizeof(ArcLabel) == sizeof(void*),
    "ArcLabel must be pointer-sized");
#endif

#if LV_USE_IME_PINYIN
static_assert(sizeof(IMEPinyin) == sizeof(void*),
    "IMEPinyin must be pointer-sized");
#endif

#if LV_USE_3DTEXTURE
static_assert(sizeof(Texture3D) == sizeof(void*),
    "Texture3D must be pointer-sized");
#endif

#if LV_USE_FILE_EXPLORER
static_assert(sizeof(FileExplorer) == sizeof(void*),
    "FileExplorer must be pointer-sized");
#endif

// ==================== Others ====================

#if LV_USE_MONKEY
static_assert(sizeof(Monkey) == sizeof(void*),
    "Monkey must be pointer-sized");
#endif

#if LV_USE_FRAGMENT
// FragmentManager is [[deprecated]] in LVGL 9.5 — no need to verify its layout
#endif

#if LV_USE_SVG
static_assert(sizeof(svg::Node) == sizeof(void*),
    "svg::Node must be pointer-sized");

static_assert(sizeof(svg::RenderObj) == sizeof(void*),
    "svg::RenderObj must be pointer-sized");
#endif

// ==================== State Types ====================

#if LV_USE_OBSERVER
static_assert(sizeof(IntState) <= sizeof(lv_subject_t) + sizeof(int32_t) + alignof(lv_subject_t),
    "IntState has excessive overhead");

static_assert(sizeof(BoolState) <= sizeof(lv_subject_t) + sizeof(bool) + alignof(lv_subject_t),
    "BoolState has excessive overhead");

static_assert(sizeof(ColorState) <= sizeof(lv_subject_t) + sizeof(lv_color_t) + alignof(lv_subject_t),
    "ColorState has excessive overhead");
#endif

// ==================== Verification Complete ====================

/// Dummy constant to verify this header was included
constexpr bool all_widgets_verified = true;

} // namespace lv::verify
