#pragma once

#include "buttons.hpp"
#include "nexHmiConfig.hpp"
#include "statusBar.hpp"
#include "nex.hpp"

namespace server {

using namespace nex::comp;

class Application;

struct WaitPage : nex::Page<0> {
    HMI_PAGE_CFG(wait);

    explicit WaitPage(nex::IAppUI& app) noexcept
        : Page<0>(app, HMI_COMP_OBJNAME(wait), PageCfg::kPageId)
    {}
};

struct WorkPage : nex::Page<57> {
    HMI_PAGE_CFG(work);

    HMI_INPLACE_PAGE_RANGE_CFG(GroupButtons, work, Button<>, bS0, bS7)
    HMI_INPLACE_PAGE_RANGE_CFG(GroupAssignButtons, work, Button<>, bSa0, bSa7)
    HMI_INPLACE_PAGE_RANGE_CFG(CellButtons, work, WinchButton, b0, b24)


    HMI_COMP(Button<>, bFile);
    HMI_COMP(Button<>, bBlock);
    HMI_COMP(Button<>, bShow);

    GroupButtons groupBtn{*this};
    GroupAssignButtons groupAssignBtn{*this};
    CellButtons cells{*this};

    HMI_COMP(Text<>, tgPage);
    HMI_COMP(Button<>, bSNext);
    HMI_COMP(Button<>, bSPrev);

    HMI_COMP(Text<>, tSt);
    HMI_COMP(Text<>, tTime);
    HMI_COMP(Text<>, tFile);
    HMI_COMP(StringVar<32>, gName);

    explicit WorkPage(nex::IAppUI& app) noexcept
        : Page<57>(app, HMI_COMP_OBJNAME(work), PageCfg::kPageId)
    {}

    void onTouch(const nex::msg::evTouch& e) override;
    void onLoad() override;

private:
    [[nodiscard]] Application& ui() const noexcept;

    void onCellPress(uint8_t index);
    void onGroupPress(uint8_t index);
    void onGroupAssignPress(uint8_t index);
    void onSceneNext();
    void onScenePrev();
    void onFilePress();
    void onBlockPress();
    void onShowPress();

    void refreshScenePageLabel() noexcept;
    void clearCellSelection() noexcept;

    bool blockMode_{false};
    uint8_t scenePage_{0u};
    uint8_t selectedScene_{0xFFu};
};

struct MGroupPage : nex::Page<8> {
    HMI_PAGE_CFG(mGroup);

    HMI_COMP(ConsoleButton, bClr);
    HMI_COMP(ConsoleButton, bRen);
    HMI_COMP(ConsoleButton, bRec);
    HMI_COMP(NumericVar, gbid);
    
    HMI_COMP(Text<>, tTime);

    explicit MGroupPage(nex::IAppUI& app) noexcept
        : Page<8>(app, HMI_COMP_OBJNAME(mGroup), PageCfg::kPageId)
    {}
};

struct MFilePage : nex::Page<9> {
    HMI_PAGE_CFG(mFile);

    HMI_COMP(ConsoleButton, bSave);
    HMI_COMP(ConsoleButton, bNew);

    HMI_COMP(Text<>, tTime);

    explicit MFilePage(nex::IAppUI& app) noexcept
        : Page<9>(app, HMI_COMP_OBJNAME(mFile), PageCfg::kPageId)
    {}
};

struct BrowserPage : nex::Page<37> {
    HMI_PAGE_CFG(browser);

    HMI_INPLACE_PAGE_RANGE_CFG(FileRows, browser, Text<>, bF0, bF7)
    HMI_INPLACE_PAGE_RANGE_CFG(FileDateRows, browser, Text<>, bF0d, bF7d)

    HMI_COMP(ConsoleButton, bAction);
    HMI_COMP(ConsoleButton, bFNext);
    HMI_COMP(ConsoleButton, bFPrev);

    HMI_COMP(ConsoleButton, b0);

    HMI_COMP(NumericVar, mode);
    HMI_COMP(StringVar<32>, fNameStr);

    HMI_COMP(Text<>, tSt);
    HMI_COMP(Text<>, tFile);
    HMI_COMP(Text<>, tTime);

    FileRows fileRows{*this};
    FileDateRows fileDates{*this};

    explicit BrowserPage(nex::IAppUI& app) noexcept
        : Page<37>(app, HMI_COMP_OBJNAME(browser), PageCfg::kPageId)
    {}
};

/** HMI UI: страницы приложения + служебные клавиатуры keybdA/keybdB. */
class Application : public nex::AppUI<nex::hmi::kPageCount> {
public:
    explicit Application(BIF::IByteStream& stream, nex::Rect screen, nex::AppTiming timing) noexcept
        : nex::AppUI<nex::hmi::kPageCount>(stream, screen, timing)
        , statusBar(screen)
        , wait(*this)
        , work(*this)
        , mGroup(*this)
        , mFile(*this)
        , browser(*this)
    {}

    /** Показать overlay UI (статус-бар и т.п.). */
    void boot() noexcept { statusBar.show(overlay); }

    StatusBar statusBar;
    WaitPage wait;
    WorkPage work;
    MGroupPage mGroup;
    MFilePage mFile;
    BrowserPage browser;
};

} // namespace server
