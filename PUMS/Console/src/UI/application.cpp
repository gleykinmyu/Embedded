#include "application.hpp"

#include <cstdio>

#include "board.hpp"
#include "core/memstat.hpp"

namespace server {

Application::Application(BIF::IByteStream& stream, nex::Rect screen, nex::AppTiming timing) noexcept
    : nex::AppUI<nex::hmi::kPageCount>(stream, screen, timing)
    , statusBar(screen)
    , wait(*this)
    , work(*this)
    , mGroup(*this)
    , mFile(*this)
    , browser(*this)
{}

void Application::boot() noexcept
{
    restartScreen();
    switchPage(0);
    statusBar.show(overlay);
    refreshStatusBar();
}

void Application::update() noexcept
{
    nex::AppUI<nex::hmi::kPageCount>::update();
    memstat::trackFreeMin();

    const uint32_t now = nowMs();
    if ((now - _statusBarTickMs) < 1000u) {
        return;
    }
    _statusBarTickMs = now;
    refreshStatusBar();
}

void Application::showFileMsg(uint8_t tag, const char* text) noexcept
{
    msgBox.setRoute(nex::Route{currentPage(), 0u});
    msgBox.show("File", nex::ovl::MsgBox::Preset::OK, tag,
        nex::ovl::MsgBox::Action::Ok, "%s", text);
}

void Application::showFileYesNo(uint8_t tag, const char* text) noexcept
{
    msgBox.setRoute(nex::Route{currentPage(), 0u});
    msgBox.show("File", nex::ovl::MsgBox::Preset::YesNo, tag,
        nex::ovl::MsgBox::Action::No, "%s", text);
}

void Application::showConsoleStatus(uint8_t tag) noexcept
{
    showFileMsg(tag, MConsole::statusText(console.getStatus()));
}

void Application::showBrowserStatus(uint8_t tag) noexcept
{
    showFileMsg(tag, MBrowser::statusText(mBrowser.getStatus()));
}

void Application::refreshStatusBar() noexcept
{
    char memBuf[16]{};
    memstat::formatFreeMin(memBuf, sizeof memBuf);
    statusBar.setStatus(memBuf);
    memstat::resetFreeMin();

    statusBar.setFile(MConsole::showBaseName(console.showName()), console.isEdited());

    char timeBuf[16]{};
    if (board.rtc.isReady()) {
        PHL::DateTime dt{};
        if (board.rtc.get(dt)) {
            std::snprintf(timeBuf, sizeof(timeBuf), "%02u:%02u:%02u",
                static_cast<unsigned>(dt.hour),
                static_cast<unsigned>(dt.minute),
                static_cast<unsigned>(dt.second));
        }
    }
    statusBar.setTime(timeBuf);
}

} // namespace server
