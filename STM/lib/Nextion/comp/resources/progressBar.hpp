#pragma once

#include "../../core/nexTypes.hpp"
#include "../nexAttributes.hpp"
#include "../nexComponentBase.hpp"

namespace nex::resources {

/** Фон ProgressBar: `bco` (Color) / `bpic` (Image). */
template<BGStyle S>
struct PbBackground;

template<>
struct PbBackground<BGStyle::Color> {
    enum Tag : uint8_t {
        Color = 48u,
    };

    static constexpr BGStyle kStyle = BGStyle::Color;
    Component& owner;

    explicit PbBackground(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"bco"}, Tag::Color, v);
    }
};

template<>
struct PbBackground<BGStyle::Image> {
    enum Tag : uint8_t {
        Image = 49u,
    };

    static constexpr BGStyle kStyle = BGStyle::Image;
    Component& owner;

    explicit PbBackground(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setImage(PicId v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"bpic"}, Tag::Image, v);
    }
};

/** Заливка ProgressBar: `pco`/`dis` (Color) / `ppic` (Image). */
template<BGStyle S>
struct PbBar;

template<>
struct PbBar<BGStyle::Color> {
    enum Tag : uint8_t {
        Color = 56u,
        CornerRadius,
    };

    static constexpr BGStyle kStyle = BGStyle::Color;
    Component& owner;

    explicit PbBar(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"pco"}, Tag::Color, v);
    }

    void setCornerRadius(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"dis"}, Tag::CornerRadius, v);
    }
};

template<>
struct PbBar<BGStyle::Image> {
    enum Tag : uint8_t {
        Image = 57u,
    };

    static constexpr BGStyle kStyle = BGStyle::Image;
    Component& owner;

    explicit PbBar(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setImage(PicId v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"ppic"}, Tag::Image, v);
    }
};

} // namespace nex::resources
