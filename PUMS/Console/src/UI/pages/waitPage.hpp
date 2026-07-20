#pragma once

#include "UI/nexHmiConfig.hpp"
#include "nex.hpp"

namespace server {

struct WaitPage : nex::Page<0> {
    HMI_PAGE_CFG(wait);

    explicit WaitPage(nex::IAppUI& app) noexcept;
};

} // namespace server
