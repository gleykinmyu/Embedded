#include "ovlButton.hpp"

namespace nex::ovl {

Button::Button(const char* const label, const Rect size, const ButtonStyle style) noexcept
    : _label(label), _style(style) {
    Object::setSize(size);
    layout();
}

void Button::setLabel(const char* const label) noexcept {
    _label = label;
    layout();
}

void Button::setStyle(const ButtonStyle style) noexcept {
    _style = style;
    layout();
}

void Button::layout() noexcept {
    Object::setSize(resolveSize(_region.size));
}

Rect Button::resolveSize(const Rect requested) const noexcept {
    uint16_t w = requested.w == 0u ? _region.size.w : requested.w;
    uint16_t h = requested.h == 0u ? _region.size.h : requested.h;
    const Font& font = _style.normal.font;
    const uint16_t minW = font.minWidthFor(_label);
    if (w < minW)
        w = minW;
    const uint16_t minH = font.minHeightFor();
    if (h < minH)
        h = minH;
    return Rect(w, h);
}

void Button::draw(const AppCanvas& cs) const {
    if (!isVisible())
        return;

    const Region screen = screenRegion();
    const TextBoxStyle& box = _pressed ? _style.pressed : _style.normal;
    cs.text_in_region_bordered(screen, _label, box.font.id, box.font.color, HAlign::Center, VAlign::Center,
        box.bg, box.border.color, box.border.thickness, BG::Color);
}

bool Button::onTouchXY(const msg::evTouchXY& e) noexcept {
    _pressed = (e.state == TouchState::Press);
    return true;
}

} // namespace nex::ovl
