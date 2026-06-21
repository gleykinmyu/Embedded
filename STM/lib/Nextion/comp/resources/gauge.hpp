#pragma once

#include "../../core/nexTypes.hpp"
#include "../nexAttributes.hpp"
#include "../nexComponentBase.hpp"

namespace nex::resources {

/** Центр Gauge: `pco2`, `left`, `hig` (NIS type 122). */
struct GaugeCenter {
    static constexpr uint8_t kDiameterMin = 0u;
    static constexpr uint8_t kDiameterMax = 100u;

    Component& owner;

    explicit GaugeCenter(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Pco2, v);
    }

    void setOffset(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Left, v);
    }

    void setDiameter(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Hig,
            clamp(v, kDiameterMin, kDiameterMax));
    }
};

/** Стрелка Gauge: `pco`, `up`, `down`, `vvs0`…`vvs2`. */
struct GaugePointer {
    static constexpr uint8_t kWidthMin = 0u;
    static constexpr uint8_t kWidthMax = 128u;

    Component& owner;

    explicit GaugePointer(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Pco, v);
    }

    void setHeadLength(int16_t v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Up, v);
    }

    void setFootLength(int16_t v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Down, v);
    }

    void setHeadWidth(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Vvs0,
            clamp(v, kWidthMin, kWidthMax));
    }

    void setBodyWidth(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Vvs1,
            clamp(v, kWidthMin, kWidthMax));
    }

    void setFootWidth(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Vvs2,
            clamp(v, kWidthMin, kWidthMax));
    }
};

} // namespace nex::resources
