#pragma once

#include "background.hpp"
#include "font.hpp"

namespace nex::resources {

/** Нажатое состояние: только фон (`bco2` / `pic2` / `picc2`). */
template<BG S = BG::Color>
struct PressedBg {
    Component& owner;
    Background<S, 2u> bg;

    explicit PressedBg(Component& ownerIn) noexcept
        : owner{ownerIn}
        , bg{ownerIn}
    {}
};

/** Нажатое состояние кнопки: фон + цвет текста (`pco2`). */
template<BG S = BG::Color>
struct Pressed : PressedBg<S> {
    FontColor<2u> font;

    explicit Pressed(Component& ownerIn) noexcept
        : PressedBg<S>{ownerIn}
        , font{ownerIn}
    {}
};

/** Состояние «вкл»: `.bg` → `bco2`, `setMarkerColor` → `pco2`. */
struct PressedMarker : PressedBg<> {
    explicit PressedMarker(Component& ownerIn) noexcept
        : PressedBg<>{ownerIn}
    {}

    void setMarkerColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(PressedBg<>::owner, attr::Id::Pco2, v);
    }
};

} // namespace nex::resources
