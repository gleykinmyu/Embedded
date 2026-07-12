#pragma once

#include "buttons.hpp"
#include "nexHmiConfig.hpp"
#include "nex.hpp"

namespace server {

struct MFilePage : nex::Page<9> {
    HMI_PAGE_CFG(mFile);

    HMI_COMP(ConsoleBtn, bSave);
    HMI_COMP(ConsoleBtn, bNew);

    explicit MFilePage(nex::IAppUI& app) noexcept;
};

} // namespace server
