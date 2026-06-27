#pragma once

#include "../../core/nexTypes.hpp"
#include "../nexAttributes.hpp"
#include "../nexComponentBase.hpp"

/** Фон виджета (`bco`/`pic`/`picc` и индексы 1/2) по `BG` и `sta`. */

namespace nex::resources {

namespace bg_detail {

template<uint8_t index>
inline constexpr attr::Id colorId() noexcept
{
    if constexpr (index == 0u)
        return attr::Id::Bco;
    else if constexpr (index == 1u)
        return attr::Id::Bco1;
    else if constexpr (index == 2u)
        return attr::Id::Bco2;
    else
        static_assert(index <= 2u, "nex::resources::Background: index must be 0..2");
}

template<uint8_t index>
inline constexpr attr::Id imageId() noexcept
{
    if constexpr (index == 0u)
        return attr::Id::Pic;
    else if constexpr (index == 1u)
        return attr::Id::Pic1;
    else if constexpr (index == 2u)
        return attr::Id::Pic2;
    else
        static_assert(index <= 2u, "nex::resources::Background: index must be 0..2");
}

template<uint8_t index>
inline constexpr attr::Id cropId() noexcept
{
    if constexpr (index == 0u)
        return attr::Id::Picc;
    else if constexpr (index == 1u)
        return attr::Id::Picc1;
    else if constexpr (index == 2u)
        return attr::Id::Picc2;
    else
        static_assert(index <= 2u, "nex::resources::Background: index must be 0..2");
}

} // namespace bg_detail

template<BG S, uint8_t index = 0u>
struct Background;

template<uint8_t index>
struct Background<BG::Color, index> {
    static constexpr BG kStyle = BG::Color;
    Component& owner;

    explicit Background(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, bg_detail::colorId<index>(), v);
    }
};

template<uint8_t index>
struct Background<BG::Image, index> {
    static constexpr BG kStyle = BG::Image;
    Component& owner;

    explicit Background(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setImage(PicId v) noexcept
    {
        attr_detail::assignNumeric(owner, bg_detail::imageId<index>(), v);
    }
};

template<uint8_t index>
struct Background<BG::CropImage, index> {
    static constexpr BG kStyle = BG::CropImage;
    Component& owner;

    explicit Background(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setCrop(PicId v) noexcept
    {
        attr_detail::assignNumeric(owner, bg_detail::cropId<index>(), v);
    }
};

template<uint8_t index>
struct Background<BG::Transparent, index> {
    static constexpr BG kStyle = BG::Transparent;

    explicit Background(Component&) noexcept {}
};

} // namespace nex::resources
