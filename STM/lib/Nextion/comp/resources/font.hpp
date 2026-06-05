#pragma once

#include "../../core/nexTypes.hpp"
#include "../nexAttributes.hpp"
#include "../nexComponentBase.hpp"

namespace nex::resources {

struct Font {
    enum Tag : uint8_t {
        Id = 48u,
        Color,
        Spacing,
    };

    Component& owner;

    explicit Font(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setId(FontId v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"font"}, Tag::Id, v);
    }

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"pco"}, Tag::Color, v);
    }

    void setCharSpacing(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"spax"}, Tag::Spacing, v);
    }
};

} // namespace nex::resources
