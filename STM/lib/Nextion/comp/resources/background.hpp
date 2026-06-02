#pragma once

#include "../../core/nexTypes.hpp"
#include "../nexAttributes.hpp"
#include "../nexComponentBase.hpp"

namespace nex::resources {

namespace bg_detail {

inline constexpr uint8_t kTagStride = 56u;

template<uint8_t baseTag, uint8_t index>
inline constexpr uint8_t tag() noexcept
{
    static_assert(index <= 2u, "nex::resources::Background: index must be 0..2");
    return static_cast<uint8_t>(baseTag + index * kTagStride);
}

template<uint8_t index>
inline constexpr Literal colorName() noexcept
{
    if constexpr (index == 0u)
        return Literal{"bco"};
    else if constexpr (index == 1u)
        return Literal{"bco1"};
    else if constexpr (index == 2u)
        return Literal{"bco2"};
    else
        static_assert(index <= 2u, "nex::resources::Background: index must be 0..2");
}

template<uint8_t index>
inline constexpr Literal imageName() noexcept
{
    if constexpr (index == 0u)
        return Literal{"pic"};
    else if constexpr (index == 1u)
        return Literal{"pic1"};
    else if constexpr (index == 2u)
        return Literal{"pic2"};
    else
        static_assert(index <= 2u, "nex::resources::Background: index must be 0..2");
}

template<uint8_t index>
inline constexpr Literal cropName() noexcept
{
    if constexpr (index == 0u)
        return Literal{"picc"};
    else if constexpr (index == 1u)
        return Literal{"picc1"};
    else if constexpr (index == 2u)
        return Literal{"picc2"};
    else
        static_assert(index <= 2u, "nex::resources::Background: index must be 0..2");
}

} // namespace bg_detail

struct ProgressBarBackground {
    enum Tag : uint8_t {
        Bpic = 193u,
    };

    Component& owner;

    explicit ProgressBarBackground(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setImage(PicId v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"bpic"}, Tag::Bpic, v);
    }

    bool onResponse(uint8_t tag, const msg::getNumeric& response) noexcept
    {
        (void)tag;
        (void)response;
        return false;
    }
};

template<BGStyle S, uint8_t index = 0u>
struct Background;

template<uint8_t index>
struct Background<BGStyle::Color, index> {
    enum Tag : uint8_t {
        Color = bg_detail::tag<32u, index>(),
    };

    static constexpr BGStyle kStyle = BGStyle::Color;
    Component& owner;

    explicit Background(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, bg_detail::colorName<index>(), Tag::Color, v);
    }

    bool onResponse(uint8_t tag, const msg::getNumeric& response) noexcept
    {
        (void)tag;
        (void)response;
        return false;
    }
};

template<uint8_t index>
struct Background<BGStyle::Image, index> {
    enum Tag : uint8_t {
        Image = bg_detail::tag<33u, index>(),
    };

    static constexpr BGStyle kStyle = BGStyle::Image;
    Component& owner;

    explicit Background(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setImage(PicId v) noexcept
    {
        attr_detail::assignNumeric(owner, bg_detail::imageName<index>(), Tag::Image, v);
    }

    bool onResponse(uint8_t tag, const msg::getNumeric& response) noexcept
    {
        (void)tag;
        (void)response;
        return false;
    }
};

template<uint8_t index>
struct Background<BGStyle::CropImage, index> {
    enum Tag : uint8_t {
        Crop = bg_detail::tag<34u, index>(),
    };

    static constexpr BGStyle kStyle = BGStyle::CropImage;
    Component& owner;

    explicit Background(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setCrop(PicId v) noexcept
    {
        attr_detail::assignNumeric(owner, bg_detail::cropName<index>(), Tag::Crop, v);
    }

    bool onResponse(uint8_t tag, const msg::getNumeric& response) noexcept
    {
        (void)tag;
        (void)response;
        return false;
    }
};

template<uint8_t index>
struct Background<BGStyle::Transparent, index> {
    static constexpr BGStyle kStyle = BGStyle::Transparent;

    explicit Background(Component&) noexcept {}

    bool onResponse(uint8_t tag, const msg::getNumeric& response) noexcept
    {
        (void)tag;
        (void)response;
        return false;
    }
};

} // namespace nex::resources
