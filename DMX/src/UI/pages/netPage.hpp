#pragma once

/**
 * Страница net — IP и адресация Art-Net / sACN.
 *
 *   bBack
 *   IP / MASK / GW : по четыре Number (nIp0…3, nMask0…3, nGw0…3) + btDhcp
 *   Art-Net: nArtNet (0–127), nArtSub (0–15), nArtUni (0–15)
 *   sACN:    nSacnUni (1–63999), nPri (0–200), btMcast, tName
 *   bOk
 */

#include "UI/nexHmiConfig.hpp"
#include "nex.hpp"

namespace ui {

using namespace nex::comp;

struct NetPage : nex::Page<28> {
    HMI_PAGE_CFG(net);

    using Octets = InplaceArray<Number<>, PG::nIp0, PG::nIp1, PG::nIp2, PG::nIp3>;
    using Masks = InplaceArray<Number<>, PG::nMask0, PG::nMask1, PG::nMask2, PG::nMask3>;
    using Gws = InplaceArray<Number<>, PG::nGw0, PG::nGw1, PG::nGw2, PG::nGw3>;

    HMI_COMP(Button<>, bBack);
    HMI_COMP(DualStateButton<>, btDhcp);
    Octets ip{*this};
    Masks mask{*this};
    Gws gw{*this};
    HMI_COMP(Number<>, nArtNet);
    HMI_COMP(Number<>, nArtSub);
    HMI_COMP(Number<>, nArtUni);
    HMI_COMP(Number<>, nSacnUni);
    HMI_COMP(Number<>, nPri);
    using NameText = Text<nex::BG::Color, 32u>;
    HMI_COMP(NameText, tName);
    HMI_COMP(DualStateButton<>, btMcast);
    HMI_COMP(Text<>, tIp);
    HMI_COMP(Text<>, tMask);
    HMI_COMP(Text<>, tGw);
    HMI_COMP(Text<>, tArt);
    HMI_COMP(Text<>, tSacn);
    HMI_COMP(Text<>, tSrc);
    HMI_COMP(Button<>, bOk);

    explicit NetPage(nex::IAppUI& app) noexcept
        : Page<28>(app, "net", 1u)
    {
    }

    void onTouch(const nex::msg::evTouch& e) override
    {
        Page<28>::onTouch(e);
        if (e.state != nex::TouchState::Release)
            return;
        if (e.route.comp == static_cast<uint8_t>(PG::bBack))
            app.switchPage(static_cast<uint8_t>(nex::hmi::Page_monitor::kPageId));
    }
};

} // namespace ui
