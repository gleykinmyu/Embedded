#pragma once

#include "browserPage.hpp"
#include "mFilePage.hpp"
#include "mGroupPage.hpp"
#include "statusBar.hpp"
#include "waitPage.hpp"
#include "workPage.hpp"
#include "nex.hpp"

#include "mconsole.hpp"

namespace server {

/** HMI UI: страницы приложения + служебные клавиатуры keybdA/keybdB. */
class Application : public nex::AppUI<nex::hmi::kPageCount> {
public:
    explicit Application(BIF::IByteStream& stream, nex::Rect screen, nex::AppTiming timing) noexcept;

    /** Показать overlay UI (статус-бар и т.п.). */
    void boot() noexcept;

   // StatusBar statusBar;
    WaitPage wait;
    WorkPage work;
    MGroupPage mGroup;
    MFilePage mFile;
    BrowserPage browser;
};



} // namespace server

extern MConsole console;