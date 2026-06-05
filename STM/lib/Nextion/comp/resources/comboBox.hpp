#pragma once

#include "../../core/nexTypes.hpp"
#include "../nexAttributes.hpp"
#include "../nexComponentBase.hpp"

namespace nex::resources {

/** Рамка ComboBox: `borderc`, `borderw`. */
struct ComboBorder {
    enum Tag : uint8_t {
        Color = 48u,
        Width,
    };

    Component& owner;

    explicit ComboBorder(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"borderc"}, Tag::Color, v);
    }

    void setWidth(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"borderw"}, Tag::Width, v);
    }
};

/** Стрелка ComboBox: `up`, `pco3`. */
struct ComboArrow {
    enum Tag : uint8_t {
        Visible = 56u,
        Color,
    };

    Component& owner;

    explicit ComboArrow(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setVisible(bool on) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"up"}, Tag::Visible, on);
    }

    void show() noexcept { setVisible(true); }
    void hide() noexcept { setVisible(false); }

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"pco3"}, Tag::Color, v);
    }
};

/** Ячейки раскрывающегося списка ComboBox. */
struct ComboCells {
    enum Tag : uint8_t {
        Dir = 64u,
        Qty,
        Spacing,
        Marker,
        MarkerSize,
        MarkerSpacing,
        BgColor,
        Color,
        SelBgColor,
        SelColor,
        CornerRadius,
    };

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
        attr_detail::assignNumeric(owner, Literal{"dir"}, Tag::Dir, static_cast<uint8_t>(v));
    }

    void setExpandCount(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"qty"}, Tag::Qty,
            attr_detail::clamp(v, kExpandCountMin, kExpandCountMax));
    }

    void setSpacing(int16_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"vvs0"}, Tag::Spacing, v);
    }

    void setMarker(ComboMarker v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"mode"}, Tag::Marker, static_cast<uint8_t>(v));
    }

    void setMarkerSize(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"wid"}, Tag::MarkerSize,
            attr_detail::clamp(v, kMarkerSizeMin, kMarkerSizeMax));
    }

    void setMarkerSpacing(int16_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"vvs1"}, Tag::MarkerSpacing, v);
    }

    void setBgColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"bco1"}, Tag::BgColor, v);
    }

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"pco1"}, Tag::Color, v);
    }

    void setSelBgColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"bco2"}, Tag::SelBgColor, v);
    }

    void setSelColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"pco2"}, Tag::SelColor, v);
    }

    void setCornerRadius(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, Literal{"dis"}, Tag::CornerRadius, v);
    }
};

} // namespace nex::resources
