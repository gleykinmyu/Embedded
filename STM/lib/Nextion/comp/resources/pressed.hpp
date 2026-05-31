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

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"pco2"}, Tag::Color, v);
    }

    bool onResponse(uint8_t tag, const msg::getNumeric& response) noexcept
    {
        (void)tag;
        (void)response;
        return false;
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

    bool onResponse(uint8_t tag, const msg::getNumeric& response) noexcept
    {
        if (bg.onResponse(tag, response))
            return true;
        return font.onResponse(tag, response);
    }
};

} // namespace nex::resources
