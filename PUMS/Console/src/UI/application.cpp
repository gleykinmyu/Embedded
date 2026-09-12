#include "application.hpp"

#include <cstdio>

#include "board.hpp"
#include "core/memstat.hpp"
#include "enc.hpp"
#include "UI/uiMessages.hpp"

namespace server {

namespace {

void onConsoleShowChanged() noexcept
{
    app.syncStatusBarFile();
}

void onConsoleMechChanged(uint8_t mech_id) noexcept
{
    app.work.onMechTelemetry(mech_id);
}

void onConsoleSelectAck() noexcept
{
    app.work.onSelectAck();
}

void onConsoleNack() noexcept
{
    if (console.lastNackReq() == smcp::msg::MsgId::Select) {
        app.work.onSelectNack();
    }
    app.showSmcpNack();
}

void onConsolePhase(smcp::IConsole::Phase /*phase*/) noexcept
{
    app.syncStatusBarLink();
}

} // namespace

Application::Application(BIF::IByteStream& stream, nex::Rect screen, nex::AppTiming timing) noexcept
    : AppUI(stream, screen, timing)
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

    static char ok[12]{};
    static char yes[12]{};
    static char no[12]{};
    static char cancel[12]{};
    enc::utf8ToOem(ok, sizeof ok, uiMsg::kBtnOk);
    enc::utf8ToOem(yes, sizeof yes, uiMsg::kBtnYes);
    enc::utf8ToOem(no, sizeof no, uiMsg::kBtnNo);
    enc::utf8ToOem(cancel, sizeof cancel, uiMsg::kBtnCancel);
    msgBox.setLabels({ok, yes, no, cancel});

    statusBar.show(overlay);
    console.setOnShowChanged(&onConsoleShowChanged);
    console.setOnMechChanged(&onConsoleMechChanged);
    console.setOnSelectAck(&onConsoleSelectAck);
    console.setOnNack(&onConsoleNack);
    console.setOnPhase(&onConsolePhase);
    syncStatusBarFile();
    syncStatusBarLink();
    syncStatusBarTime();
}

void Application::onPageChange(const nex::msg::evPage& e) noexcept
{
    const uint8_t prev = currentPage();

    /* keybdB → settings: правка даты/времени (key открывает клавиатуру без Press MCU). */
    if (prev == nex::hmi::kKeybdBPageId && e.page == nex::hmi::Page_settings::kPageId) {
        settings.noteReturnFromKeybd();
    }

    AppUI::onPageChange(e);
    overlay.redrawShownWidgets();
}

void Application::update() noexcept
{
    AppUI::update();
    memstat::trackFreeMin();

    const uint32_t now = nowMs();
    if ((now - _statusBarTickMs) < 1000u) {
        return;
    }
    _statusBarTickMs = now;
    syncStatusBarLink();
    syncStatusBarTime();
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

void Application::showSmcpNack(uint8_t tag) noexcept
{
    showUtf8Msg(uiMsg::kTitleSmcp, nex::ovl::MsgBox::Preset::OK, tag,
        nex::ovl::MsgBox::Action::Ok, MConsole::nackText(console.lastNack()));
}

void Application::syncStatusBarFile() noexcept
{
    statusBar.setFile(MConsole::showBaseName(console.showName()), console.isEdited());
}

void Application::syncStatusBarLink() noexcept
{
    char buf[32]{};
    const unsigned freeKb =
        static_cast<unsigned>((memstat::freeMinBytes() + 512u) / 1024u);
    std::snprintf(buf, sizeof(buf), "%s (%uk)",
        smcp::IConsole::cstr(console.phase()), freeKb);
    statusBar.setStatus(buf);
    memstat::resetFreeMin();
}

void Application::syncStatusBarTime() noexcept
{
    PHL::DateTime dt{};
    if (board.rtc.isReady() && board.rtc.get(dt)) {
        statusBar.setTime(dt);
    }
}

} // namespace server
