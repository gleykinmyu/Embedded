#pragma once

#include <cstdint>
#include <new>

#include "nex.hpp"
#include "nexHmiConfig.hpp"

namespace server {

using namespace nex::comp;

/** Страница pgWork (panel page id 5), 47 виджетов id 1…47. */
struct PgWorkPage : nex::Page<47> {
    HMI_PAGE_CFG(pgWork);

    static constexpr uint8_t kButtonCount = 24u;
    static constexpr uint8_t kButtonIdFirst = 2u; // b0 … b23 → panel id 2…25

    /** b0…b23; `b[i]` — panel id `kButtonIdFirst + i`. */
    struct ButtonRow {
        alignas(Button<>) unsigned char mem[sizeof(Button<>) * kButtonCount];

        Button<> &operator[](uint8_t i) noexcept
        {
            return reinterpret_cast<Button<> *>(mem)[i];
        }

        const Button<> &operator[](uint8_t i) const noexcept
        {
            return reinterpret_cast<const Button<> *>(mem)[i];
        }

        explicit ButtonRow(nex::IPage &page) noexcept
        {
#define X(sym, id) new (&(*this)[(id) - kButtonIdFirst]) Button<>(page, HMI_COMP_OBJNAME(sym), HMI_COMP_ID(PageCfg, sym));
            X(b0, 2) X(b1, 3) X(b2, 4) X(b3, 5) X(b4, 6) X(b5, 7) X(b6, 8) X(b7, 9)
            X(b8, 10) X(b9, 11) X(b10, 12) X(b11, 13) X(b12, 14) X(b13, 15) X(b14, 16) X(b15, 17)
            X(b16, 18) X(b17, 19) X(b18, 20) X(b19, 21) X(b20, 22) X(b21, 23) X(b22, 24) X(b23, 25)
#undef X
        }

        ~ButtonRow()
        {
            for (uint8_t i = 0; i < kButtonCount; ++i)
                (*this)[i].~Button<>();
        }
    };

    ButtonRow b{*this};
    HMI_WIDGET(Button<>, ctrlClear)
    HMI_WIDGET(Button<>, ctrlSelAll)
    HMI_WIDGET(Button<>, bS0)
    HMI_WIDGET(Button<>, bS1)
    HMI_WIDGET(Button<>, bS2)
    HMI_WIDGET(Button<>, bS3)
    HMI_WIDGET(Button<>, bS4)
    HMI_WIDGET(Button<>, bS5)
    HMI_WIDGET(Button<>, bS6)
    HMI_WIDGET(Button<>, bS7)
    HMI_WIDGET(Button<>, bSNext)
    HMI_WIDGET(Button<>, bSPrev)
    HMI_WIDGET(Button<>, mFile)
    HMI_WIDGET(Text<>, t0)
    HMI_WIDGET(Text<>, tSt)
    HMI_WIDGET(Text<>, tPg)
    HMI_WIDGET(DualStateButton<>, bsmScene)
    HMI_WIDGET(Timer, tmrBlock)
    HMI_WIDGET(Button<>, sBlock)
    HMI_WIDGET(StringVar<48>, varScnName)
    HMI_WIDGET(Text<>, t1)
    HMI_WIDGET(Slider<>, sShow)

    explicit PgWorkPage(nex::IAppUI &app) noexcept
        : Page<47>(app, "pgWork", PageCfg::kPageId)
    {
    }
};

} // namespace server
