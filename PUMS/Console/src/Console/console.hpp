#pragma once

#include <cstdint>

#include "nex.hpp"
#include "nexHmiConfig.hpp"

namespace server {

using namespace nex::comp;

/** Страница pgWork (panel page id 5), 47 виджетов id 1…47. */
struct PgWorkPage : nex::Page<47> {
    HMI_PAGE_CFG(pgWork);

    /** b0…b23; `b[i]` — panel id `b.kIdFirst + i`. */
    HMI_INPLACE_PAGE_RANGE_CFG(PgWorkButtons, pgWork, Button<>, b0, b23)
    PgWorkButtons b;

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
        , b(*this)
    {}
};

} // namespace server
