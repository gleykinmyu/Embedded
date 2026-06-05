#pragma once

#include "../../core/nexTypes.hpp"
#include "../nexAttributes.hpp"
#include "../nexComponentBase.hpp"

namespace nex::resources {

namespace font_detail {

inline constexpr uint8_t kTagStride = 56u;

template<uint8_t baseTag, uint8_t index>
inline constexpr uint8_t tag() noexcept
{
    static_assert(index <= 2u, "nex::resources::FontColor: index must be 0..2");
    return static_cast<uint8_t>(baseTag + index * kTagStride);
}

template<uint8_t index>
inline constexpr Literal colorName() noexcept
{
    if constexpr (index == 0u)
        return Literal{"pco"};
    else if constexpr (index == 1u)
        return Literal{"pco1"};
    else if constexpr (index == 2u)
        return Literal{"pco2"};
    else
        static_assert(index <= 2u, "nex::resources::FontColor: index must be 0..2");
}

} // namespace font_detail

/** Цвет глифов (NIS `pco` / `pco1` / `pco2`). */
template<uint8_t index = 0u>
struct FontColor {
    enum Tag : uint8_t {
        Color = font_detail::tag<48u, index>(),
    };

    Component& owner;

    explicit FontColor(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, font_detail::colorName<index>(), Tag::Color, v);
    }
};

/** + идентификатор шрифта (NIS `font`). */
template<uint8_t index = 0u>
struct FontId : FontColor<index> {
    enum Tag : uint8_t {
        Id = 49u,
    };

    explicit FontId(Component& ownerIn) noexcept
        : FontColor<index>{ownerIn}
    {}

    void setId(nex::FontId v) noexcept
    {
        attr_detail::assignNumeric(FontColor<index>::owner, Literal{"font"}, Tag::Id, v);
    }
};

/** + межсимвольный интервал (NIS `spax`). */
template<uint8_t index = 0u>
struct Font : FontId<index> {
    enum Tag : uint8_t {
        Spacing = 50u,
    };

    explicit Font(Component& ownerIn) noexcept
        : FontId<index>{ownerIn}
    {}

    void setCharSpacing(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(FontId<index>::owner, Literal{"spax"}, Tag::Spacing, v);
    }
};

} // namespace nex::resources
