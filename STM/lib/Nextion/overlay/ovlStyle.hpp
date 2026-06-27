#pragma once

#include "../core/nexTypes.hpp"

namespace nex::ovl {

/** Фон виджета: заливка, рамка, опционально `pic` / crop (0 — без картинки). */
/** Стили кнопки: шрифт, подпись, заливка / рамка. */
struct BorderStyle {
    Color color{Color::std::White};
    uint16_t thickness = 0u;
    constexpr BorderStyle() noexcept = default;
    constexpr BorderStyle(Color color, uint16_t thickness) noexcept : color(color), thickness(thickness) {}
};

struct BoxStyle {
    Color bg{Color::std::White};
    BorderStyle border;

    constexpr BoxStyle() noexcept = default;
    constexpr BoxStyle(Color bg, Color borderColor, uint16_t borderThickness) noexcept : bg(bg), border(borderColor, borderThickness) {}
};

struct ClrFont : public Font {
    Color color{Color::std::White};
    constexpr ClrFont() noexcept = default;
    constexpr ClrFont(FontId Id, uint16_t heightPx, Color color) noexcept : Font(Id, heightPx), color(color) {}
    constexpr ClrFont(Font font, Color color) noexcept : Font(font), color(color) {}
};

struct TextBoxStyle : public BoxStyle {
    ClrFont font{0u, 32u, Color::std::Black};

    constexpr TextBoxStyle() noexcept = default;
    constexpr TextBoxStyle(Color bg, Color bColor, uint16_t bThickness, FontId fontId, uint16_t fontHeightPx, Color fColor) noexcept : BoxStyle(bg, bColor, bThickness), font(fontId, fontHeightPx, fColor) {}
    constexpr TextBoxStyle(Color bg, Color bColor, uint16_t bThickness, Font font, Color fColor) noexcept : BoxStyle(bg, bColor, bThickness), font(font, fColor) {}
};

} // namespace nex::ovl
