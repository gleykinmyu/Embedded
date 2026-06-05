#pragma once

#include "../../core/nexTypes.hpp"
#include "../nexAttributes.hpp"
#include "../nexComponentBase.hpp"

namespace nex::resources {

/** Центр Gauge: `pco1`, `left`, `hig`. */
struct GaugeCenter {
    enum Tag : uint8_t {
        Color = 48u,
        Offset,
        Diameter,
    };

    static constexpr uint8_t kDiameterMin = 0u;
    static constexpr uint8_t kDiameterMax = 100u;

    Component& owner;

    explicit GaugeCenter(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"pco1"}, Tag::Color, v);
    }

    void setOffset(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"left"}, Tag::Offset, v);
    }

    void setDiameter(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"hig"}, Tag::Diameter,
            attr_detail::clamp(v, kDiameterMin, kDiameterMax));
    }
};

/** Стрелка Gauge: `pco`, `up`, `down`, `vvs0`…`vvs2`. */
struct GaugePointer {
    enum Tag : uint8_t {
        Color = 56u,
        HeadLength,
        FootLength,
        HeadWidth,
        BodyWidth,
        FootWidth,
    };

    static constexpr uint8_t kWidthMin = 0u;
    static constexpr uint8_t kWidthMax = 128u;

    Component& owner;

    explicit GaugePointer(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"pco"}, Tag::Color, v);
    }

    void setHeadLength(int16_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"up"}, Tag::HeadLength, v);
    }

    void setFootLength(int16_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"down"}, Tag::FootLength, v);
    }

    void setHeadWidth(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"vvs0"}, Tag::HeadWidth,
            attr_detail::clamp(v, kWidthMin, kWidthMax));
    }

    void setBodyWidth(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"vvs1"}, Tag::BodyWidth,
            attr_detail::clamp(v, kWidthMin, kWidthMax));
    }

    void setFootWidth(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"vvs2"}, Tag::FootWidth,
            attr_detail::clamp(v, kWidthMin, kWidthMax));
    }
};

} // namespace nex::resources
