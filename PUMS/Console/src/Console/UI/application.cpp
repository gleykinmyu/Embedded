#include "application.hpp"

#include <cstdio>

#include "board.hpp"
#include "core/memstat.hpp"
#include "enc.hpp"
#include "UI/uiMessages.hpp"

namespace server {

namespace {

[[nodiscard]] const char* nackText(smcp::msg::ErrorCode code) noexcept
{
    switch (code) {
    case smcp::msg::ErrorCode::Busy: return uiMsg::kSmcpBusy;
    case smcp::msg::ErrorCode::Limits: return uiMsg::kSmcpLimits;
    case smcp::msg::ErrorCode::Crc: return uiMsg::kSmcpCrc;
    case smcp::msg::ErrorCode::MechNotFound: return uiMsg::kSmcpMechNotFound;
    case smcp::msg::ErrorCode::Safety: return uiMsg::kSmcpSafety;
    case smcp::msg::ErrorCode::NotReady: return uiMsg::kSmcpNotReady;
    case smcp::msg::ErrorCode::SelectLimit: return uiMsg::kSmcpSelectLimit;
    case smcp::msg::ErrorCode::Timeout: return uiMsg::kSmcpTimeout;
    case smcp::msg::ErrorCode::Ok:
    default: return uiMsg::kSmcpError;
    }
}

[[nodiscard]] const char* fileSystemText(smcp::file::Status st) noexcept
{
    switch (st) {
    case smcp::file::Status::BadMagic: return uiMsg::kConsoleBadMagic;
    case smcp::file::Status::BadVersion: return uiMsg::kConsoleBadVersion;
    case smcp::file::Status::BadHeaderCrc:
    case smcp::file::Status::BadBodyCrc: return uiMsg::kConsoleBadCrc;
    case smcp::file::Status::BadLayout: return uiMsg::kConsoleBadLayout;
    case smcp::file::Status::Truncated: return uiMsg::kConsoleTruncated;
    case smcp::file::Status::IoError:
    default: return uiMsg::kStorageError;
    }
}

[[nodiscard]] const char* fileSystemText(smcp::file::IBrowser::Status st) noexcept
{
    switch (st) {
    case smcp::file::IBrowser::Status::NotMounted: return uiMsg::kBrowserNotMounted;
    case smcp::file::IBrowser::Status::OpenDirFailed: return uiMsg::kBrowserOpenDirFailed;
    case smcp::file::IBrowser::Status::InvalidName: return uiMsg::kBrowserInvalidName;
    case smcp::file::IBrowser::Status::NotFound: return uiMsg::kBrowserNotFound;
    case smcp::file::IBrowser::Status::FileExists: return uiMsg::kBrowserFileExists;
    case smcp::file::IBrowser::Status::PathTooLong: return uiMsg::kBrowserPathTooLong;
    case smcp::file::IBrowser::Status::IoError:
    default: return uiMsg::kStorageError;
    }
}

[[nodiscard]] const char* fileSystemText(smcp::file::FIOManager::Status st) noexcept
{
    using St = smcp::file::FIOManager::Status;
    switch (st) {
    case St::NoShowOpen: return uiMsg::kConsoleNoShowOpen;
    case St::MissingSection: return uiMsg::kConsoleMissingGrup;
    case St::InvalidData: return uiMsg::kConsoleBadGroups;
    case St::OpenFileProtected: return uiMsg::kBrowserOpenProtected;
    case St::BrowserFail: return fileSystemText(console.browser.status());
    case St::MainFail:
    case St::BakFail:
    case St::RestoreFail: {
        const smcp::file::Status fs = (console.show.status() != smcp::file::Status::Ok)
            ? console.show.status()
            : smcp::file::Status::IoError;
        return fileSystemText(fs);
    }
    case St::Ok:
    default: return uiMsg::kOk;
    }
}

} // namespace

void UiConsole::onMechChanged(uint8_t mech_id) noexcept
{
    app.work.onMechTelemetry(mech_id);
}

void UiConsole::onConsoleChanged() noexcept
{
    app.syncStatusBarFile();
    app.syncStatusBarLink();
}

void UiConsole::onGroupAck(uint8_t /*group_id*/) noexcept
{
    app.work.onGroupAck();
}

void UiConsole::onNack(smcp::Session* session, const smcp::TxSlot& req,
                       const smcp::msg::Nack& reply) noexcept
{
    MConsole::onNack(session, req, reply);
    app.showSmcpNack();
}

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
    syncStatusBarFile();
    syncStatusBarLink();
    syncStatusBarTime();
    _sdNoDisk = (board.SD.status() & STA_NODISK) != 0;
}

void Application::goWork(bool syncScene) noexcept
{
    if (syncScene) {
        work.refreshGroups(true);
        work.refreshCells();
    }
    switchPage(work);
}

void Application::onUiReady() noexcept
{
    work.refreshGroups(true);
    console.setUiReady();
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

    const bool noDisk = (board.SD.status() & STA_NODISK) != 0;
    if (noDisk != _sdNoDisk
        && currentPage() == nex::hmi::Page_browser::kPageId && !overlay.isModal()) {
        browser.reloadOnCardChange();
    }
    _sdNoDisk = noDisk;
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

void Application::showFileSystemStatus(uint8_t tag) noexcept
{
    showFileMsg(tag, fileSystemText(console.fio.status()));
}

void Application::showBrowserStatus(uint8_t tag) noexcept
{
    showFileMsg(tag, fileSystemText(console.browser.status()));
}

void Application::showSmcpNack(uint8_t tag) noexcept
{
    showUtf8Msg(uiMsg::kTitleSmcp, nex::ovl::MsgBox::Preset::OK, tag,
        nex::ovl::MsgBox::Action::Ok,
        nackText(static_cast<smcp::msg::ErrorCode>(console.lastNack().error)));
}

void Application::syncStatusBarFile() noexcept
{
    statusBar.setFile(smcp::file::FIOManager::showBaseName(console.show.name()), console.show.isEdited());
    mFile.refreshSaveBtn();
}

void Application::syncStatusBarLink() noexcept
{
    char buf[32]{};
    const unsigned freeKb =
        static_cast<unsigned>((memstat::freeMinBytes() + 512u) / 1024u);
    std::snprintf(buf, sizeof(buf), "%s %s (%uk)",
        (board.SD.status() & STA_NODISK) != 0 ? "--" : "SD",
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
