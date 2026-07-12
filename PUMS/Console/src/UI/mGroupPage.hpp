#pragma once

#include "buttons.hpp"
#include "nexHmiConfig.hpp"
#include "nex.hpp"

namespace server {

struct MGroupPage : nex::Page<7> {
    HMI_PAGE_CFG(mGroup);

    HMI_COMP(ConsoleBtn, bClr);
    HMI_COMP(ConsoleBtn, bRen);
    HMI_COMP(ConsoleBtn, bRec);
    HMI_COMP(nex::comp::NumericVar, gbid);

    explicit MGroupPage(nex::IAppUI& app) noexcept;
};

} // namespace server
