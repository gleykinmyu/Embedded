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

#include "model/mconsole.hpp"

namespace server {

class UiConsole final : public MConsole {
public:
    using MConsole::MConsole;

protected:
    void onMechChanged(uint8_t mech_id) noexcept override;
    void onConsoleChanged() noexcept override;
    void onGroupAck(uint8_t group_id) noexcept override;
    void onNack(smcp::Session* session, const smcp::TxSlot& req,
                const smcp::msg::Nack& reply) noexcept override;
};

/** HMI UI: страницы приложения + служебные клавиатуры keybdA/keybdB. */
class Application : public nex::AppUI<nex::hmi::kPageCount> {
public:
    using AppUI = nex::AppUI<nex::hmi::kPageCount>;

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
    /** MsgBox «Файл» по статусу FIO (браузер / шоуфайл). */
    void showFileSystemStatus(uint8_t tag = 0u) noexcept;
    /** MsgBox «Файл» по статусу браузера. */
    void showBrowserStatus(uint8_t tag = 0u) noexcept;
    /** MsgBox по последнему SMCP Nack. */
    void showSmcpNack(uint8_t tag = 0u) noexcept;

    /** На work. `syncScene` — группы/tgPage и клетки до `page work`. */
    void goWork(bool syncScene = false) noexcept;

    /** wait.onLoad: группы/tgPage, затем Online + телеметрия (клетки). */
    void onUiReady() noexcept;

    void onPageChange(const nex::msg::evPage& e) noexcept override;

    void syncStatusBarFile() noexcept;
    /** Phase + free RAM, напр. `Online (92k)`. */
    void syncStatusBarLink() noexcept;
    void syncStatusBarTime() noexcept;

private:
    uint32_t _statusBarTickMs = 0u;
};

} // namespace server

extern server::UiConsole console;
extern server::Application app;
