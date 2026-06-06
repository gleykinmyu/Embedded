#pragma once

#include "../core/nexCommands.hpp"
#include "../core/nexTypes.hpp"

namespace nex {

class Application;

/** Фасад `cmd::gui::*` и связанные системные переменные — поле `Application::cs`. */
class AppCanvas {
    friend class Application;

    Application& _app;

public:
    /** Межсимвольный интервал текста (`spax`). */
    void setCharSpacing(uint16_t spacing) const noexcept;
    /** Межстрочный интервал текста (`spay`). */
    void setLineSpacing(uint16_t spacing) const noexcept;
    /** Цвет кисти touch-рисования (`thc`). */
    void setTouchDrawColor(Color color) const noexcept;
    /** Вкл./выкл. touch-рисование (`thdra`). */
    void drawOnTouch(bool enable) const noexcept;

    void clear_screen(Color color) const noexcept;
    void picture(Point at, PicId pictureId) const noexcept;
    void picture_in_place(Region region, PicId pictureId) const noexcept;
    void picture_in_place(Point upperLeft, uint32_t w, uint32_t h, PicId pictureId) const noexcept;
    void picture_draw(Point dst, Region src, PicId pictureId) const noexcept;
    void picture_draw(Point dst, uint32_t w, uint32_t h, Point src, PicId pictureId) const noexcept;

    /** `xstr` в `region`: текст → шрифт/цвет/выравнивание → фон (по умолчанию прозрачный). */
    void text_in_region(Region region, const char* contentToken, FontId fontId = 1u,
        Color fg = Color::std::White, HAlign hAlign = HAlign::Center, VAlign vAlign = VAlign::Center,
        Color bg = Color::std::Black, BG fill = BG::Transparent) const noexcept;
    /** То же, с равномерным отступом `pad` со всех сторон. */
    void text_in_region(Region region, uint16_t pad, const char* contentToken, FontId fontId = 1u,
        Color fg = Color::std::White, HAlign hAlign = HAlign::Center, VAlign vAlign = VAlign::Center,
        Color bg = Color::std::Black, BG fill = BG::Transparent) const noexcept;
    /** `rect_bordered`/`rect_fill` + `text_in_region` в одном `region`. */
    void text_in_region_bordered(Region region, const char* contentToken, FontId fontId, Color fg, HAlign hAlign,
        VAlign vAlign, Color fill, Color border, uint16_t borderThickness,
        BG textFill = BG::Color) const noexcept;

    /** Прямоугольник: заливка `color`. */
    void rect_fill(Point upperLeft, Point lowerRightInclusive, Color color) const noexcept;
    void rect_fill(Point upperLeft, Rect size, Color color) const noexcept;
    void rect_fill(Region rect, Color color) const noexcept;
    
    /** Прямоугольник: рамка `color`. */
    void rect_outline(Point upperLeft, Point lowerRightInclusive, Color color) const noexcept;
    void rect_outline(Point upperLeft, Rect size, Color color) const noexcept;
    void rect_outline(Region rect, Color color) const noexcept;
    /** Прямоугольник: заливка `fill`, рамка `border` толщиной `borderThickness` пикс. (углы в любом порядке). */
    void rect_bordered(Point upperLeft, Point lowerRightInclusive, Color fill, Color border, uint16_t borderThickness) const noexcept;
    void rect_bordered(Point upperLeft, Rect size, Color fill, Color border, uint16_t borderThickness) const noexcept;
    void rect_bordered(Region rect, Color fill, Color border, uint16_t borderThickness) const noexcept;
    /** Линия: `from` → `to` цветом `color`. */
    void line(Point from, Point to, Color color) const noexcept;
    /** Круг: контур `color`. */
    void circle_outline(Point center, uint16_t radius, Color color) const noexcept;
    /** Круг: заливка `color`. */
    void circle_filled(Point center, uint16_t radius, Color color) const noexcept;

    explicit AppCanvas(Application& a) noexcept;
};

namespace Canvas {

/** Два угла (порядок не важен) → верхний левый и нижний правый (включительно). */
void normalizeRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, Point& upperLeft,
    Point& lowerRightInclusive) noexcept;

/** Два угла → `Region` (`ul` + `size`). */
[[nodiscard]] Region region(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) noexcept;

/** Точка `p` внутри `region` (включительно по границе); при нулевом размере — false. */
[[nodiscard]] bool contains(Region region, Point p) noexcept;

/** Верхний левый угол при центрировании `box` на `screen` (целочисленное `(screen - box) / 2`). */
[[nodiscard]] Point center(const Rect& screen, const Rect& box) noexcept;

/** Внутренняя область `outer` после отступа `borderThickness` с каждой стороны. */
[[nodiscard]] Region innerRegion(const Region& outer, uint16_t borderThickness) noexcept;

/** Кнопка в координатах экрана: заливка + подпись (`xstr`). */
class Button {
public:
    Button() noexcept = default;
    /** `size` расширяется, если уже для `label` и `font`. `borderThickness` == 0 — без рамки. */
    Button(const char* label, Font font, Rect size, Color bgColor, Color pressedColor, Color borderColor,
        uint16_t borderThickness) noexcept;

    /** Размер до `show` (`w`/`h` == 0 → из текущего `_region.size`). */
    void setSize(Rect size) noexcept;
    void setStyle(Color bgColor, Color pressedColor, Color borderColor, uint16_t borderThickness) noexcept;

    void show(Point upperLeft) noexcept;
    void show() noexcept;
    void hide() noexcept;
    void draw(const AppCanvas& cs, bool pressed) const noexcept;
    [[nodiscard]] bool contains(Point p) const noexcept;
    [[nodiscard]] bool isVisible() const noexcept;

    [[nodiscard]] const Region& region() const noexcept { return _region; }
    [[nodiscard]] Rect size() const noexcept { return _region.size; }
    [[nodiscard]] uint16_t w() const noexcept { return _region.size.w; }
    [[nodiscard]] uint16_t h() const noexcept { return _region.size.h; }

    /** Разместить справа от `left` (размер из `_region.size`). */
    void placeRight(const Button& left, uint16_t gap) noexcept;

private:
    /** `w`/`h` == 0 → из `_region.size`; иначе не меньше минимума по подписи и шрифту. */
    [[nodiscard]] Rect resolveSize(Rect requested) const noexcept;

    Region _region;
    const char* _label = nullptr;
    Font _font;
    Color _bgColor{Color::std::Gray};
    Color _pressedColor{Color::std::Green};
    Color _borderColor{Color::std::White};
    uint16_t _borderThickness = 0u;
    bool _visible = false;
};

} // namespace Canvas

} // namespace nex
