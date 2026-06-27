#pragma once

#include "../../core/nexTypes.hpp"
#include "../nexAttributes.hpp"
#include "../nexComponentBase.hpp"

/** Курсор Slider: `wid`, `hig` (NIS). */

namespace nex::resources {

struct Cursor {
    Component& owner;

    explicit Cursor(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setWidth(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Wid, v);
    }

    void setHeight(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Hig, v);
    }

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Pco, v);
    }
};

} // namespace nex::resources
