#include "waitPage.hpp"

#include "UI/application.hpp"

namespace server {

WaitPage::WaitPage(nex::IAppUI& app) noexcept
    : Page<0>(app, HMI_COMP_OBJNAME(wait), PG::kPageId)
{}

Application& WaitPage::ui() const noexcept
{
    return static_cast<Application&>(app);
}

void WaitPage::onLoad()
{
    ui().onUiReady();
}

} // namespace server
