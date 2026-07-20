#pragma once

#include "pages/browserPage.hpp"
#include "buttons.hpp"
#include "pages/mFilePage.hpp"
#include "pages/mGroupPage.hpp"
#include "pages/settingsPage.hpp"
#include "statusBar.hpp"
#include "pages/waitPage.hpp"
#include "pages/workPage.hpp"
#include "nex.hpp"
#include "overlay/ovl.hpp"

#include "mconsole.hpp"
#include "mbrowser.hpp"

namespace server {

/** HMI UI: страницы приложения + служебные клавиатуры keybdA/keybdB. */
class Application : public nex::AppUI<nex::hmi::kPageCount> {
public:
    explicit Application(BIF::IByteStream& stream, nex::Rect screen, nex::AppTiming timing) noexcept;

    /** Показать overlay UI (статус-бар и т.п.). */
    void boot() noexcept;

    void update() noexcept override;

    nex::ovl::MsgBox msgBox{*this, kAppMsgBoxColors};

    StatusBar statusBar;
    WaitPage wait;
    WorkPage work;
    MGroupPage mGroup;
    MFilePage mFile;
    BrowserPage browser;
    SettingsPage settings;

    void showFileMsg(uint8_t tag, const char* text) noexcept;
    void showFileYesNo(uint8_t tag, const char* text) noexcept;
    void showGroupMsg(uint8_t tag, const char* text) noexcept;
    void showGroupYesNo(uint8_t tag, const char* text) noexcept;
    /**
     * MsgBox: @a titleUtf8 / @a textUtf8 в UTF-8 (как в редакторе) → KOI8-R на панель.
     * Не передавать сюда имена с SD — они уже OEM.
     */
    void showUtf8Msg(const char* titleUtf8, nex::ovl::MsgBox::Preset preset, uint8_t tag,
        nex::ovl::MsgBox::Action defaultAction, const char* textUtf8) noexcept;
    /** MsgBox по текущему статусу MConsole. */
    void showConsoleStatus(uint8_t tag = 0u) noexcept;
    /** MsgBox по статусу MConsole с заголовком «Группа». */
    void showGroupStatus(uint8_t tag = 0u) noexcept;
    /** MsgBox по текущему статусу MBrowser. */
    void showBrowserStatus(uint8_t tag = 0u) noexcept;

    void onPageChange(const nex::msg::evPage& e) noexcept override;

private:
    void refreshStatusBar() noexcept;

    uint32_t _statusBarTickMs = 0u;
};

} // namespace server

extern MConsole console;
extern MBrowser mBrowser;
