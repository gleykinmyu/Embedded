#pragma once

#include "../../core/nexTypes.hpp"
#include "../nexAttributes.hpp"
#include "../nexComponentBase.hpp"
#include <cstdint>

namespace nex::resources {

/** Атрибуты шрифта NIS (`font`, `pco`, `spax`) на printable-ветке компонентов. */
struct Font {
    enum Tag : uint8_t {
        Id = 48u,
        Color,
        Spacing,
    };

    attr::Num<FontId> id;
    attr::Num<nex::Color> color;
    attr::Num<uint8_t> spacing;

    explicit Font(Component& owner) noexcept
        : id{owner, "font", Tag::Id}
        , color{owner, "pco", Tag::Color}
        , spacing{owner, "spax", Tag::Spacing}
    {}

    bool onResponse(uint8_t tag, const msg::getNumeric& response) noexcept
    {
        switch (tag) {
        case Tag::Id:
            id.applyResponse(response);
            return true;
        case Tag::Color:
            color.applyResponse(response);
            return true;
        case Tag::Spacing:
            spacing.applyResponse(response);
            return true;
        default:
            return false;
        }
    }
};

} // namespace nex::resources
