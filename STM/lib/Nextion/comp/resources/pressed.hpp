#pragma once

#include "background.hpp"

namespace nex::resources {

/** Цвет шрифта в нажатом состоянии (`pco2`). */
struct PressedFont {
    enum Tag : uint8_t {
        Color = 147u,
    };

    attr::Num<nex::Color> color;

    explicit PressedFont(Component& owner) noexcept
        : color{owner, "pco2", Tag::Color}
    {}

    bool onResponse(uint8_t tag, const msg::getNumeric& response) noexcept
    {
        if (tag == Tag::Color) {
            color.applyResponse(response);
            return true;
        }
        return false;
    }
};

/** Состояние «нажато»: фон и цвет текста. */
template<BGStyle S>
struct Pressed {
    Background<S, true> bg;
    PressedFont font;

    explicit Pressed(Component& owner) noexcept
        : bg{owner}
        , font{owner}
    {}

    bool onResponse(uint8_t tag, const msg::getNumeric& response) noexcept
    {
        if (bg.onResponse(tag, response))
            return true;
        return font.onResponse(tag, response);
    }
};

} // namespace nex::resources
