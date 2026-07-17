#pragma once

#include "browserPage.hpp"
#include "buttons.hpp"
#include "mFilePage.hpp"
#include "mGroupPage.hpp"
#include "statusBar.hpp"
#include "waitPage.hpp"
#include "workPage.hpp"
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

    void showFileMsg(uint8_t tag, const char* text) noexcept;
    void showFileYesNo(uint8_t tag, const char* text) noexcept;
    /** MsgBox по текущему статусу MConsole. */
    void showConsoleStatus(uint8_t tag = 0u) noexcept;
    /** MsgBox по текущему статусу MBrowser. */
    void showBrowserStatus(uint8_t tag = 0u) noexcept;

private:
    void refreshStatusBar() noexcept;

    uint32_t _statusBarTickMs = 0u;
};

} // namespace server

extern MConsole console;
extern MBrowser mBrowser;
