#pragma once

#include "../../core/nexTypes.hpp"
#include "../nexAttributes.hpp"
#include "../nexComponentBase.hpp"

namespace nex::resources {

namespace font_detail {

template<uint8_t index>
inline constexpr attr::Id colorId() noexcept
{
    if constexpr (index == 0u)
        return attr::Id::Pco;
    else if constexpr (index == 1u)
        return attr::Id::Pco1;
    else if constexpr (index == 2u)
        return attr::Id::Pco2;
    else
        static_assert(index <= 2u, "nex::resources::FontColor: index must be 0..2");
}

} // namespace font_detail

/** Цвет глифов (NIS `pco` / `pco1` / `pco2`). */
template<uint8_t index = 0u>
struct FontColor {
    Component& owner;

    explicit FontColor(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, font_detail::colorId<index>(), v);
    }
};

/** + идентификатор шрифта (NIS `font`). */
template<uint8_t index = 0u>
struct FontId : FontColor<index> {
    explicit FontId(Component& ownerIn) noexcept
        : FontColor<index>{ownerIn}
    {}

    void setId(nex::FontId v) noexcept
    {
        attr_detail::assignNumeric(FontColor<index>::owner, attr::Id::Font, v);
    }
};

/** + межсимвольный интервал (NIS `spax`). */
template<uint8_t index = 0u>
struct Font : FontId<index> {
    explicit Font(Component& ownerIn) noexcept
        : FontId<index>{ownerIn}
    {}

    void setCharSpacing(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(FontId<index>::owner, attr::Id::Spax, v);
    }
};

} // namespace nex::resources
