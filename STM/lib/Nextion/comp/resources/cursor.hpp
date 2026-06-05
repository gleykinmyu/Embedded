#pragma once

#include "../../core/nexTypes.hpp"
#include "../nexAttributes.hpp"
#include "../nexComponentBase.hpp"

namespace nex::resources {

struct Cursor {
    enum Tag : uint8_t {
        W = 193u,
        H,
        Color,
    };

    Component& owner;

    explicit Cursor(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setWidth(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"wid"}, Tag::W, v);
    }

    void setHeight(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"hig"}, Tag::H, v);
    }

    void setThumbColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"pco"}, Tag::Color, v);
    }
};

} // namespace nex::resources
