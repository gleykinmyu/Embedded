#pragma once

#include "buttons.hpp"
#include "nexHmiConfig.hpp"
#include "nex.hpp"

namespace server {

struct BrowserPage : nex::Page<37> {
    HMI_PAGE_CFG(browser);

    HMI_INPLACE_PAGE_RANGE_CFG(FileRows, browser, nex::comp::Text<>, bF0, bF7)
    HMI_INPLACE_PAGE_RANGE_CFG(FileDateRows, browser, nex::comp::Text<>, bF0d, bF7d)

    HMI_COMP(ConsoleBtn, bAction);
    HMI_COMP(ConsoleBtn, bFNext);
    HMI_COMP(ConsoleBtn, bFPrev);

    HMI_COMP(ConsoleBtn, b0);

    HMI_COMP(nex::comp::NumericVar, mode);
    HMI_COMP(nex::comp::StringVar<32>, fNameStr);

    FileRows fileRows{*this};
    FileDateRows fileDates{*this};

    explicit BrowserPage(nex::IAppUI& app) noexcept;
};

} // namespace server
