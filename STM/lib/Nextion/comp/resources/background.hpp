#pragma once

#include "../../core/nexTypes.hpp"
#include "../nexAttributes.hpp"
#include "../nexComponentBase.hpp"

namespace nex::resources {

/**
 * Поля фона NIS (`sta`): состав зависит от `BGStyle` (compile-time).
 * `Pressed == true` — атрибуты нажатого состояния (`bco2`, `pic2`, `picc2`).
 */
template<BGStyle S, bool Pressed = false>
struct Background;

template<bool Pressed>
struct Background<BGStyle::Color, Pressed> {
    enum Tag : uint8_t {
        Color = Pressed ? 144u : 32u,
    };

    static constexpr BGStyle kStyle = BGStyle::Color;

    attr::Num<nex::Color> color;

    explicit Background(Component& owner) noexcept
        : color{owner, Pressed ? "bco2" : "bco", Tag::Color}
    {}

    bool onResponse(uint8_t tag, const msg::getNumeric& response) noexcept
    {
        if (tag == Tag::Color) {
            color.applyResponse(response);
            return true;
        }
        return false;
    }
};

template<bool Pressed>
struct Background<BGStyle::Image, Pressed> {
    enum Tag : uint8_t {
        Image = Pressed ? 145u : 33u,
    };

    static constexpr BGStyle kStyle = BGStyle::Image;

    attr::Num<PicId> image;

    explicit Background(Component& owner) noexcept
        : image{owner, Pressed ? "pic2" : "pic", Tag::Image}
    {}

    bool onResponse(uint8_t tag, const msg::getNumeric& response) noexcept
    {
        if (tag == Tag::Image) {
            image.applyResponse(response);
            return true;
        }
        return false;
    }
};

template<bool Pressed>
struct Background<BGStyle::CropImage, Pressed> {
    enum Tag : uint8_t {
        Crop = Pressed ? 146u : 34u,
    };

    static constexpr BGStyle kStyle = BGStyle::CropImage;

    attr::Num<PicId> crop;

    explicit Background(Component& owner) noexcept
        : crop{owner, Pressed ? "picc2" : "picc", Tag::Crop}
    {}

    bool onResponse(uint8_t tag, const msg::getNumeric& response) noexcept
    {
        if (tag == Tag::Crop) {
            crop.applyResponse(response);
            return true;
        }
        return false;
    }
};

template<bool Pressed>
struct Background<BGStyle::Transparent, Pressed> {
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
