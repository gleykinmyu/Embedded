#pragma once

#include "UI/nexHmiConfig.hpp"
#include "nex.hpp"

namespace server {

class Application;

struct WaitPage : nex::Page<0> {
    HMI_PAGE_CFG(wait);

    explicit WaitPage(nex::IAppUI& app) noexcept;

    void onLoad() override;

private:
    [[nodiscard]] Application& ui() const noexcept;
};

} // namespace server
