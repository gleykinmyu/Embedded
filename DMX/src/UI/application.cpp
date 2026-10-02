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
    _gotPage = true;
    NEX_DBG("Nextion 0x66 evPage page=%u\n", static_cast<unsigned>(e.page));
    AppUI::onPageChange(e);
    touch.sendXY(true);
}

void Application::onSystemEvent(const nex::msg::evSystem& e)
{
    NEX_DBG("Nextion evSystem code=0x%02X\n", static_cast<unsigned>(e.code));
    if (e.code == nex::msg::evSystem::Code::NextionReady
        || e.code == nex::msg::evSystem::Code::StartupPreamble)
        _gotReady = true;
}

void Application::onStatus(const nex::msg::Status& status, const nex::Route route) noexcept
{
    AppUI::onStatus(status, route);
    if (status.isAppError())
        return;
    _panelStatus = status;
    _gotPanelStatus = true;
}

bool Application::waitEvPage(const uint32_t ms) noexcept
{
    const uint32_t t0 = boardClockMs();
    while ((boardClockMs() - t0) < ms) {
        board.watchdog.kick();
        update();
        if (_gotPage)
            return true;
    }
    return _gotPage;
}

bool Application::waitPanelStatus(const uint32_t ms) noexcept
{
    const uint32_t t0 = boardClockMs();
    while ((boardClockMs() - t0) < ms) {
        board.watchdog.kick();
        update();
        if (_gotPanelStatus)
            return true;
    }
    return _gotPanelStatus;
}

void Application::showAfterLink() noexcept
{
    switchPage(monitor);
    showMonitor();
    if (!pumpUntilIdle())
        NEX_DBG("Nextion first paint stall\n");
    touch.sendXY(true);
    _linkSettled = true;
}

void Application::applyFastBaudIfNeeded() noexcept
{
    if (_linkSettled)
        return;

    if (!_restSent) {
        (void)pumpUntilIdle();
        _restSent = true;
        NEX_DBG("Nextion rest sent — wait 0x88\n");
        return;
    }

    if (!_gotPage) {
        if (!_pageAsked) {
            _pageAsked = true;
            requestCurrentPage();
            (void)pumpUntilIdle();
        }
        if (!waitEvPage(200u))
            return;
    }

    NEX_DBG("Nextion link %u OD — draw\n", static_cast<unsigned>(kLinkBaudBoot));
    showAfterLink();
}

} // namespace ui
