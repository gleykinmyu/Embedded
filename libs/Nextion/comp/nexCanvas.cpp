#include "nexCanvas.hpp"
#include "../app/nexApplication.hpp"
#include "../app/nexSysVars.hpp"
#include "../core/nexProtocol.hpp"

namespace nex {

namespace {

constexpr Literal kSysSpax{"spax"};
constexpr Literal kSysSpay{"spay"};
constexpr Literal kSysThc{"thc"};
constexpr Literal kSysThdra{"thdra"};

} // namespace

// --- Canvas rect helpers ----------------------------------------------------

void Canvas::normalizeRect(const Coord x0, const Coord y0, const Coord x1, const Coord y1, Point& upperLeft,
    Point& lowerRightInclusive) noexcept {
    const Point a(x0, y0);
    const Point b(x1, y1);
    upperLeft.x = (a.x < b.x) ? a.x : b.x;
    upperLeft.y = (a.y < b.y) ? a.y : b.y;
    lowerRightInclusive.x = (a.x > b.x) ? a.x : b.x;
    lowerRightInclusive.y = (a.y > b.y) ? a.y : b.y;
}

Region Canvas::region(const Coord x0, const Coord y0, const Coord x1, const Coord y1) noexcept {
    Region r;
    Point lr;
    normalizeRect(x0, y0, x1, y1, r.ul, lr);
    if (lr.x >= r.ul.x && lr.y >= r.ul.y) {
        r.size.w = static_cast<Coord>(lr.x - r.ul.x + 1);
        r.size.h = static_cast<Coord>(lr.y - r.ul.y + 1);
    }
    return r;
}

bool Canvas::contains(const Region region, const Point p) noexcept {
    return region.contains(p);
}

Point Canvas::center(const Rect& screen, const Rect& box) noexcept {
    return Point(static_cast<Coord>((static_cast<int32_t>(screen.w) - box.w) / 2),
        static_cast<Coord>((static_cast<int32_t>(screen.h) - box.h) / 2));
}

Region Canvas::innerRegion(const Region& outer, const uint16_t borderThickness) noexcept {
    if (outer.size.isEmpty())
        return Region();
    if (borderThickness == 0u)
        return outer;
    const int32_t inset = 2 * static_cast<int32_t>(borderThickness);
    if (static_cast<int32_t>(outer.size.w) <= inset || static_cast<int32_t>(outer.size.h) <= inset)
        return Region();
    return Region(Point(static_cast<Coord>(outer.ul.x + borderThickness),
                      static_cast<Coord>(outer.ul.y + borderThickness)),
        Rect(static_cast<Coord>(outer.size.w - inset),
            static_cast<Coord>(outer.size.h - inset)));
}

Region Canvas::toScreen(const Region& parentScreen, const Region& childLocal) noexcept {
    return Region(
        Point(static_cast<Coord>(parentScreen.ul.x + childLocal.ul.x),
            static_cast<Coord>(parentScreen.ul.y + childLocal.ul.y)),
        childLocal.size);
}

// --- AppCanvas --------------------------------------------------------------

AppCanvas::AppCanvas(Application& a) noexcept : _app(a) {}

void AppCanvas::setCharSpacing(uint16_t spacing) const noexcept {
    enqueueSysVarNumericAssign(_app, kSysSpax, static_cast<int32_t>(spacing));
}

void AppCanvas::setLineSpacing(uint16_t spacing) const noexcept {
    enqueueSysVarNumericAssign(_app, kSysSpay, static_cast<int32_t>(spacing));
}

void AppCanvas::setTouchDrawColor(Color color) const noexcept {
    enqueueSysVarNumericAssign(_app, kSysThc, static_cast<int32_t>(color.raw));
}

void AppCanvas::drawOnTouch(bool enable) const noexcept {
    enqueueSysVarNumericAssign(_app, kSysThdra, enable ? 1 : 0);
}

void AppCanvas::clear_screen(Color color) const noexcept {
    _app.enqueue(Transaction{cmd::gui::ClearScreen(color), 0u, 0u});
}

void AppCanvas::picture(Point at, PicId pictureId) const noexcept {
    _app.enqueue(Transaction{cmd::gui::Picture(at, pictureId), 0u, 0u});
}

void AppCanvas::picture_in_place(const Region region, const PicId pictureId) const noexcept {
    if (region.size.isEmpty())
        return;
    _app.enqueue(Transaction{cmd::gui::PictureCrop::inPlace(region, pictureId), 0u, 0u});
}

void AppCanvas::picture_in_place(const Point upperLeft, const Rect size,
    const PicId pictureId) const noexcept {
    picture_in_place(Region(upperLeft, size), pictureId);
}

void AppCanvas::picture_draw(const Point dst, const Region src, const PicId pictureId) const noexcept {
    if (src.size.isEmpty())
        return;
    _app.enqueue(Transaction{cmd::gui::PictureCrop::draw(dst, src, pictureId), 0u, 0u});
}

void AppCanvas::picture_draw(const Point dst, const Rect size, const Point src,
    const PicId pictureId) const noexcept {
    picture_draw(dst, Region(src, size), pictureId);
}

void AppCanvas::text_in_region(const Region region, const char* contentToken, const FontId fontId, const Color fg,
    const HAlign hAlign, const VAlign vAlign, const Color bg, const BG fill) const noexcept {
    if (contentToken == nullptr || contentToken[0] == '\0')
        return;
    if (region.size.isEmpty())
        return;
    _app.enqueue(Transaction{
        cmd::gui::TextInRegion(region, fontId, fg, bg, hAlign, vAlign, fill, contentToken), 0u, 0u});
}

void AppCanvas::text_in_region(const Region region, const uint16_t pad, const char* contentToken,
    const FontId fontId, const Color fg, const HAlign hAlign, const VAlign vAlign, const Color bg,
    const BG fill) const noexcept {
    const Coord inset = static_cast<Coord>(2 * pad);
    if (region.size.w <= inset || region.size.h <= inset)
        return;
    const Region insetR(Point(static_cast<Coord>(region.ul.x + pad), static_cast<Coord>(region.ul.y + pad)),
        Rect(static_cast<Coord>(region.size.w - inset), static_cast<Coord>(region.size.h - inset)));
    text_in_region(insetR, contentToken, fontId, fg, hAlign, vAlign, bg, fill);
}

void AppCanvas::text_in_region_bordered(const Region region, const char* const contentToken, const FontId fontId,
    const Color fg, const HAlign hAlign, const VAlign vAlign, const Color fill, const Color border,
    const uint16_t borderThickness, const BG textFill) const noexcept {
    if (region.size.isEmpty())
        return;
    if (borderThickness > 0u)
        rect_bordered(region, fill, border, borderThickness);
    else
        rect_fill(region, fill);

    const Region textRegion = Canvas::innerRegion(region, borderThickness);
    if (!textRegion.size.isEmpty())
        text_in_region(textRegion, contentToken, fontId, fg, hAlign, vAlign, fill, textFill);
}

void AppCanvas::rect_fill(const Region rect, const Color color) const noexcept {
    if (rect.size.isEmpty())
        return;
    _app.enqueue(Transaction{cmd::gui::Rect(cmd::gui::Rect::Mode::Fill, rect, color), 0u, 0u});
}

void AppCanvas::rect_fill(const Point upperLeft, const Point lowerRightInclusive, const Color color) const noexcept {
    rect_fill(Canvas::region(upperLeft.x, upperLeft.y, lowerRightInclusive.x, lowerRightInclusive.y), color);
}

void AppCanvas::rect_fill(const Point upperLeft, const Rect size, const Color color) const noexcept {
    if (size.isEmpty())
        return;
    rect_fill(Region(upperLeft, size), color);
}

void AppCanvas::rect_outline(const Region rect, const Color color) const noexcept {
    if (rect.size.isEmpty())
        return;
    _app.enqueue(Transaction{cmd::gui::Rect(cmd::gui::Rect::Mode::Outline, rect, color), 0u, 0u});
}

void AppCanvas::rect_outline(const Point upperLeft, const Point lowerRightInclusive, const Color color) const noexcept {
    rect_outline(Canvas::region(upperLeft.x, upperLeft.y, lowerRightInclusive.x, lowerRightInclusive.y), color);
}

void AppCanvas::rect_outline(const Point upperLeft, const Rect size, const Color color) const noexcept {
    if (size.isEmpty())
        return;
    rect_outline(Region(upperLeft, size), color);
}

void AppCanvas::rect_bordered(const Region outer, const Color fill, const Color border,
    const uint16_t borderThickness) const noexcept {
    if (outer.size.isEmpty())
        return;
    if (borderThickness == 0u) {
        rect_fill(outer, fill);
        return;
    }

    rect_fill(outer, border);

    const Region inner = Canvas::innerRegion(outer, borderThickness);
    if (!inner.size.isEmpty())
        rect_fill(inner, fill);
}

void AppCanvas::rect_bordered(const Point upperLeft, const Point lowerRightInclusive, const Color fill,
    const Color border, const uint16_t borderThickness) const noexcept {
    rect_bordered(
        Canvas::region(upperLeft.x, upperLeft.y, lowerRightInclusive.x, lowerRightInclusive.y), fill, border,
        borderThickness);
}

void AppCanvas::rect_bordered(const Point upperLeft, const Rect size, const Color fill, const Color border,
    const uint16_t borderThickness) const noexcept {
    if (size.isEmpty())
        return;
    rect_bordered(Region(upperLeft, size), fill, border, borderThickness);
}

void AppCanvas::line(Point from, Point to, Color color) const noexcept {
    _app.enqueue(Transaction{cmd::gui::Line(from, to, color), 0u, 0u});
}

void AppCanvas::circle_outline(const Point center, const uint16_t radius, const Color color) const noexcept {
    _app.enqueue(Transaction{cmd::gui::Circle(cmd::gui::Circle::Kind::Outline, center, radius, color), 0u, 0u});
}

void AppCanvas::circle_filled(const Point center, const uint16_t radius, const Color color) const noexcept {
    _app.enqueue(Transaction{cmd::gui::Circle(cmd::gui::Circle::Kind::Filled, center, radius, color), 0u, 0u});
}

// --- Font / Canvas::Button --------------------------------------------------

Coord Font::minWidthFor(const char* text, const uint16_t padX, const int16_t spax) const noexcept {
    const int32_t pad = 2 * static_cast<int32_t>(padX);
    if (text == nullptr || text[0] == '\0')
        return static_cast<Coord>(pad < 0 ? 0 : pad);

    const int32_t glyphW = (static_cast<int32_t>(heightPx) * 3 + 4) / 5;
    int32_t w = pad;
    const uint16_t maxChars = cmd::gui::TextInRegion::maxTextLength;
    uint16_t n = 0u;
    for (const char* p = text; *p != '\0' && n < maxChars; ++p, ++n) {
        w += glyphW;
        if (p[1] != '\0' && spax > 0 && (n + 1u) < maxChars)
            w += static_cast<int32_t>(spax);
    }
    if (w < 0)
        return 0;
    return static_cast<Coord>(w);
}

Canvas::Button::Button(const char* const label, const Font font, const Rect size, const Color bgColor,
    const Color pressedColor, const Color borderColor, const uint16_t borderThickness) noexcept
    : _label(label), _font(font), _bgColor(bgColor), _pressedColor(pressedColor), _borderColor(borderColor),
      _borderThickness(borderThickness) {
    _region.size = resolveSize(size);
}

void Canvas::Button::setStyle(const Color bgColor, const Color pressedColor, const Color borderColor,
    const uint16_t borderThickness) noexcept {
    _bgColor = bgColor;
    _pressedColor = pressedColor;
    _borderColor = borderColor;
    _borderThickness = borderThickness;
}

Rect Canvas::Button::resolveSize(Rect requested) const noexcept {
    Coord w = requested.w == 0 ? _region.size.w : requested.w;
    Coord h = requested.h == 0 ? _region.size.h : requested.h;
    return Rect(max(w, _font.minWidthFor(_label)), max(h, _font.minHeightFor()));
}

bool Canvas::Button::isVisible() const noexcept {
    return _visible;
}

void Canvas::Button::setSize(const Rect size) noexcept {
    _region.size = resolveSize(size);
}

void Canvas::Button::show(const Point upperLeft) noexcept {
    _region.ul = upperLeft;
    _visible = true;
}

void Canvas::Button::show() noexcept {
    _visible = true;
}

void Canvas::Button::hide() noexcept {
    _visible = false;
}

void Canvas::Button::placeRight(const Button& left, const uint16_t gap) noexcept {
    setSize(Rect(0, left._region.size.h));
    show(left._region.ul.right(static_cast<Coord>(left._region.size.w + gap)));
}

bool Canvas::Button::contains(const Point p) const noexcept {
    return isVisible() && _region.contains(p);
}

void Canvas::Button::draw(const AppCanvas& cs, const bool pressed) const noexcept {
    if (!isVisible())
        return;

    const Color fill = pressed ? _pressedColor : _bgColor;
    const uint16_t borderTh = (!pressed && _borderThickness > 0u) ? _borderThickness : 0u;
    if (_label != nullptr) {
        cs.text_in_region_bordered(_region, _label, _font.id, Color::std::White, HAlign::Center, VAlign::Center,
            fill, _borderColor, borderTh, BG::Color);
    } else if (borderTh > 0u) {
        cs.rect_bordered(_region, fill, _borderColor, borderTh);
    } else {
        cs.rect_fill(_region, fill);
    }
}

} // namespace nex
