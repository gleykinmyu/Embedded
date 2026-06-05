#pragma once

#include "../../core/nexTypes.hpp"
#include "../nexAttributes.hpp"
#include "../nexComponentBase.hpp"

namespace nex::resources {

struct FloatPoint {
    enum Tag : uint8_t {
        Left = 163u,
        Right,
    };

    Component& owner;

    explicit FloatPoint(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setDigitsBeforePoint(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"vvs0"}, Tag::Left, v);
    }

    void setDigitsAfterPoint(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"vvs1"}, Tag::Right, v);
    }
};

} // namespace nex::resources
