#include "UI/application.hpp"

namespace ui {

void Application::boot() noexcept
{
    restartScreen();
    switchPage(monitor);
    showMonitor();
}

void Application::showMonitor() noexcept
{
    view.showOn(overlay);
}

void Application::hideMonitor() noexcept
{
    view.hideFrom(overlay);
}

void Application::onPageChange(const nex::msg::evPage& e) noexcept
{
    AppUI::onPageChange(e);
    if (e.page == nex::hmi::Page_monitor::kPageId)
        overlay.redrawShownWidgets();
}

} // namespace ui
