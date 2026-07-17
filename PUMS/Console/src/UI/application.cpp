#include "application.hpp"

#include <cstdio>
#include <cstring>

#include "board.hpp"
#include "core/memstat.hpp"

namespace server {

namespace {

[[nodiscard]] const char* fileBaseName(const char* path) noexcept
{
    if (path == nullptr || path[0] == '\0') {
        return "";
    }
    const char* slash = std::strrchr(path, '/');
    if (slash == nullptr) {
        slash = std::strrchr(path, '\\');
    }
    return (slash != nullptr && slash[1] != '\0') ? (slash + 1) : path;
}

} // namespace

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

void Application::showFsError() noexcept
{
    showFileMsg(0u, "Storage error.");
}

void Application::refreshStatusBar() noexcept
{
    char memBuf[16]{};
    memstat::formatFreeMin(memBuf, sizeof memBuf);
    statusBar.setStatus(memBuf);
    memstat::resetFreeMin();

    statusBar.setFile(fileBaseName(console.showName()));

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
