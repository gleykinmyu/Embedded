#include "application.hpp"

#include <cstdio>

#include "board.hpp"
#include "core/memstat.hpp"
#include "enc.hpp"
#include "UI/uiMessages.hpp"

namespace server {

Application::Application(BIF::IByteStream& stream, nex::Rect screen, nex::AppTiming timing) noexcept
    : nex::AppUI<nex::hmi::kPageCount>(stream, screen, timing)
    , statusBar(screen)
    , wait(*this)
    , work(*this)
    , mGroup(*this)
    , mFile(*this)
    , browser(*this)
    , settings(*this)
{}

void Application::boot() noexcept
{
    restartScreen();
    switchPage(0);
    statusBar.show(overlay);
    refreshStatusBar();
}

void Application::onPageChange(const nex::msg::evPage& e) noexcept
{
    const uint8_t prev = currentPage();

    /* keybdB → settings: правка даты/времени (key открывает клавиатуру без Press MCU). */
    if (prev == nex::hmi::kKeybdBPageId && e.page == nex::hmi::Page_settings::kPageId) {
        settings.noteReturnFromKeybd();
    }

    nex::AppUI<nex::hmi::kPageCount>::onPageChange(e);
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

void Application::showUtf8Msg(const char* titleUtf8, nex::ovl::MsgBox::Preset preset, uint8_t tag,
    nex::ovl::MsgBox::Action defaultAction, const char* textUtf8) noexcept
{
    const enc::OemString title(titleUtf8);
    const enc::OemString body(textUtf8);
    msgBox.setRoute(nex::Route{currentPage(), 0u});
    msgBox.show(title.c_str(), preset, tag, defaultAction, "%s", body.c_str());
}

void Application::showFileMsg(uint8_t tag, const char* text) noexcept
{
    showUtf8Msg(uiMsg::kTitleFile, nex::ovl::MsgBox::Preset::OK, tag, nex::ovl::MsgBox::Action::Ok, text);
}

void Application::showFileYesNo(uint8_t tag, const char* text) noexcept
{
    showUtf8Msg(uiMsg::kTitleFile, nex::ovl::MsgBox::Preset::YesNo, tag, nex::ovl::MsgBox::Action::No, text);
}

void Application::showGroupMsg(uint8_t tag, const char* text) noexcept
{
    showUtf8Msg(uiMsg::kTitleGroup, nex::ovl::MsgBox::Preset::OK, tag, nex::ovl::MsgBox::Action::Ok, text);
}

void Application::showGroupYesNo(uint8_t tag, const char* text) noexcept
{
    showUtf8Msg(uiMsg::kTitleGroup, nex::ovl::MsgBox::Preset::YesNo, tag, nex::ovl::MsgBox::Action::No,
                text);
}

void Application::showConsoleStatus(uint8_t tag) noexcept
{
    if (console.getStatus() == MConsole::Status::BrowserFault) {
        showBrowserStatus(tag);
        return;
    }
    showFileMsg(tag, MConsole::statusText(console.getStatus()));
}

void Application::showGroupStatus(uint8_t tag) noexcept
{
    showGroupMsg(tag, MConsole::statusText(console.getStatus()));
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
