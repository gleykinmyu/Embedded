#include "UI/pages/monitorPage.hpp"

#include "UI/application.hpp"

namespace ui {

Application& MonitorPage::ui() const noexcept
{
    return static_cast<Application&>(app);
}

void MonitorPage::onLoad()
{
    ui().showMonitor();
}

void MonitorPage::onExit()
{
    ui().hideMonitor();
}

} // namespace ui
