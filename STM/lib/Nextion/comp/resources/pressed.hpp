#pragma once

#include "background.hpp"

namespace nex::resources {

struct PressedFont {
    enum Tag : uint8_t {
        Color = 147u,
    };

    Component& owner;

    explicit PressedFont(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setPressedTextColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"pco2"}, Tag::Color, v);
    }
};

template<BGStyle S>
struct Pressed {
    Background<S, 2u> bg;
    PressedFont font;

    explicit Pressed(Component& owner) noexcept
        : bg{owner}
        , font{owner}
    {}
};

} // namespace nex::resources
