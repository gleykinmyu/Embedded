#pragma once

#include "../../core/nexTypes.hpp"
#include "../nexAttributes.hpp"
#include "../nexComponentBase.hpp"

namespace nex::resources {

/** Рамка ComboBox: `borderc`, `borderw`. */
struct ComboBorder {
    Component& owner;

    explicit ComboBorder(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Borderc, v);
    }

    void setWidth(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Borderw, v);
    }
};

/** Стрелка ComboBox: `up`, `pco3`. */
struct ComboArrow {
    Component& owner;

    explicit ComboArrow(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setVisible(bool on) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Up, on);
    }

    void show() noexcept { setVisible(true); }
    void hide() noexcept { setVisible(false); }

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Pco3, v);
    }
};

/** Ячейки раскрывающегося списка ComboBox. */
struct ComboCells {
    static constexpr uint8_t kExpandCountMin = 1u;
    static constexpr uint8_t kExpandCountMax = 254u;
    static constexpr uint8_t kMarkerSizeMin = 8u;
    static constexpr uint8_t kMarkerSizeMax = 255u;

    Component& owner;

    explicit ComboCells(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setExpandDirection(ComboExpandDirection v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Dir, static_cast<uint8_t>(v));
    }

    void setExpandCount(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Qty,
            clamp(v, kExpandCountMin, kExpandCountMax));
    }

    void setSpacing(int16_t v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Vvs0, v);
    }

    void setMarker(ComboMarker v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Mode, static_cast<uint8_t>(v));
    }

    void setMarkerSize(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Wid,
            clamp(v, kMarkerSizeMin, kMarkerSizeMax));
    }

    void setMarkerSpacing(int16_t v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Vvs1, v);
    }

    void setBgColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Bco1, v);
    }

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Pco1, v);
    }

    void setSelBgColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Bco2, v);
    }

    void setSelColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Pco2, v);
    }

    void setCornerRadius(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Dis, v);
    }
};

} // namespace nex::resources
