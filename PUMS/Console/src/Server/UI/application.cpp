#include "application.hpp"

#include <cstdio>

#include "board.hpp"
#include "core/memstat.hpp"
#include "model/mserver.hpp"

extern MServer mServer;

namespace segment {

namespace {

Application* g_app = nullptr;

} // namespace

Application::ConsolePage::ConsolePage(nex::IAppUI& app) noexcept
    : Page<1>(app, HMI_COMP_OBJNAME(page0), PG::kPageId)
{}

Application::Application(BIF::IByteStream& stream, nex::Rect screen, nex::AppTiming timing) noexcept
    : AppUI(stream, screen, timing)
    , _page(*this)
    , _statusBar(screen,
          static_cast<nex::Coord>(screen.h - kCnslHeight),
          kCnslHeight)
{
    g_app = this;
}

void Application::onSerial1Log(const char* data, std::size_t len) noexcept
{
    if (g_app != nullptr) {
        g_app->_page.cnsl.write(data, len);
    }
}

void Application::boot() noexcept
{
    switchPage(0);
    requestCurrentPage();
    _page.cnsl.setText("");
    _statusBar.show(overlay);
    syncStatusBarLink();
    syncStatusBarTime();
    setSerial1LogSink(&Application::onSerial1Log);
}

void Application::onPageChange(const nex::msg::evPage& e) noexcept
{
    AppUI::onPageChange(e);
    overlay.redrawShownWidgets();
    NEX_DBG("nex page -> %u\n", static_cast<unsigned>(e.page));
}

void Application::update() noexcept
{
    AppUI::update();
    memstat::trackFreeMin();
    _page.cnsl.tick();

    const uint32_t now = nowMs();
    if ((now - _statusBarTickMs) < 1000u) {
        return;
    }
    _statusBarTickMs = now;
    syncStatusBarLink();
    syncStatusBarTime();
}

void Application::syncStatusBarLink() noexcept
{
    char buf[32]{};
    const unsigned freeKb =
        static_cast<unsigned>((memstat::freeMinBytes() + 512u) / 1024u);
    std::snprintf(buf, sizeof(buf), "%s (%uk)",
        smcp::Node::cstr(mServer.getStatus()), freeKb);
    _statusBar.setStatus(buf);
    memstat::resetFreeMin();
}

void Application::syncStatusBarTime() noexcept
{
    PHL::DateTime dt{};
    if (board.rtc.isReady() && board.rtc.get(dt)) {
        _statusBar.setTime(dt);
    }
}

} // namespace segment
