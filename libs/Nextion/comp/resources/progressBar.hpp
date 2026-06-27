#pragma once

#include "../../core/nexTypes.hpp"
#include "../nexAttributes.hpp"
#include "../nexComponentBase.hpp"

/** ProgressBar: фон (`PbBackground`) и полоса (`PbBar`) для `sta=Color|Image`. */

namespace nex::resources {

/** Фон ProgressBar: `bco` (Color) / `bpic` (Image). */
template<BG S>
struct PbBackground;

template<>
struct PbBackground<BG::Color> {
    static constexpr BG kStyle = BG::Color;
    Component& owner;

    explicit PbBackground(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Bco, v);
    }
};

template<>
struct PbBackground<BG::Image> {
    static constexpr BG kStyle = BG::Image;
    Component& owner;

    explicit PbBackground(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setImage(PicId v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Bpic, v);
    }
};

/** Заливка ProgressBar: `pco`/`dis` (Color) / `ppic` (Image). */
template<BG S>
struct PbBar;

template<>
struct PbBar<BG::Color> {
    static constexpr BG kStyle = BG::Color;
    Component& owner;

    explicit PbBar(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Pco, v);
    }

    void setCornerRadius(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Dis, v);
    }
};

template<>
struct PbBar<BG::Image> {
    static constexpr BG kStyle = BG::Image;
    Component& owner;

    explicit PbBar(Component& ownerIn) noexcept
        : owner{ownerIn}
    {}

    void setImage(PicId v) noexcept
    {
        attr_detail::assignNumeric(owner, attr::Id::Ppic, v);
    }
};

} // namespace nex::resources
