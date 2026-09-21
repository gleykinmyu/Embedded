#include "UI/application.hpp"

#include "UI/enc.hpp"
#include "board.hpp"
#include "core/nexDebug.hpp"

namespace ui {

void Application::boot() noexcept
{
    switchPage(monitor);
    showMonitor();
}

void Application::showMonitor() noexcept
{
    hideGraph();
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
    AppUI::onPageChange(e);
    if (!_fastBaud) {
        _wantFastBaud = true;
        return;
    }
    if (e.page == nex::hmi::Page_monitor::kPageId)
        overlay.redrawShownWidgets();
}

void Application::applyFastBaudIfNeeded() noexcept
{
    if (!_wantFastBaud || _fastBaud)
        return;
    _wantFastBaud = false;

    NEX_DBG("Nextion evPage — raise baud %u -> %u\n",
        static_cast<unsigned>(kLinkBaudBoot), static_cast<unsigned>(kLinkBaudFast));
    setBaudrate(kLinkBaudFast);
    if (!pumpUntilIdle())
        NEX_DBG("Nextion baud command TX timeout\n");
    _link.flush();

    const uint32_t t0 = boardClockMs();
    while ((boardClockMs() - t0) < 5u)
        board.watchdog.kick();

    _link.close();
    if (!_link.open(kLinkBaudFast)) {
        NEX_DBG("USART2 open(%u) failed — stay closed\n", static_cast<unsigned>(kLinkBaudFast));
        return;
    }
    _link.purge();
    _link.clearErrors();
    _fastBaud = true;
    NEX_DBG("Nextion link %u\n", static_cast<unsigned>(kLinkBaudFast));
    if (currentPage() == nex::hmi::Page_monitor::kPageId)
        overlay.redrawShownWidgets();
}

} // namespace ui
