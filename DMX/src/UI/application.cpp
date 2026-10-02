#include "UI/application.hpp"

#include "UI/enc.hpp"
#include "board.hpp"
#include "core/nexDebug.hpp"

namespace ui {

void Application::boot() noexcept
{
    _link.purge();
    _link.clearErrors();
    restartScreen();
}

void Application::showMonitor() noexcept
{
    hideGraph();
    if (view.isVisible())
        return;
    view.showOn(overlay);
}

void Application::hideMonitor() noexcept
{
    view.hideFrom(overlay);
}

void Application::showGraph() noexcept
{
    hideMonitor();
    graph.showOn(overlay);
}

void Application::hideGraph() noexcept
{
    graph.hideFrom(overlay);
}

void Application::refreshUi() noexcept
{
    if (!_linkSettled)
        return;
    if (_fullRedraw) {
        _fullRedraw = false;
        overlay.redrawShownWidgets();
        if (view.isVisible())
            view.noteFullRedraw();
        return;
    }
    if (graph.isVisible())
        graph.refresh();
    else if (view.isVisible())
        view.refresh();
}

void Application::onAfterMsgBox(const nex::msg::evMsgBox& e) noexcept
{
    AppUI::onAfterMsgBox(e);
    _fullRedraw = true;
}

void Application::alert(const char* const utf8) noexcept
{
    char oem[96]{};
    enc::utf8ToOem(oem, sizeof(oem), utf8);
    msgBox.show("DMX", nex::ovl::MsgBox::Preset::OK, 0u, nex::ovl::MsgBox::Action::Ok, "%s", oem);
}

void Application::onPageChange(const nex::msg::evPage& e) noexcept
{
    NEX_DBG("Nextion 0x66 evPage page=%u\n", static_cast<unsigned>(e.page));
    AppUI::onPageChange(e);
    if (!view.isVisible() && !graph.isVisible())
        showMonitor();
    overlay.redrawShownWidgets();
    if (view.isVisible())
        view.noteFullRedraw();
    touch.sendXY(true);
    _linkSettled = true;
}

void Application::onStatus(const nex::msg::Status& status, const nex::Route route) noexcept
{
    AppUI::onStatus(status, route);
}

void Application::applyFastBaudIfNeeded() noexcept
{
    if (_restSent)
        return;
    (void)pumpUntilIdle();
    _restSent = true;
    NEX_DBG("Nextion rest sent — wait 0x66 evPage\n");
}

} // namespace ui
