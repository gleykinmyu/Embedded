#include "mFilePage.hpp"

#include "UI/application.hpp"
#include "UI/uiMessages.hpp"

namespace server {

MFilePage::MFilePage(nex::IAppUI& app) noexcept
    : Page<10>(app, HMI_COMP_OBJNAME(mFile), PG::kPageId)
{}

Application& MFilePage::ui() const noexcept
{
    return static_cast<Application&>(app);
}

void MFilePage::onLoad()
{
    refreshSaveBtn();
}

void MFilePage::refreshSaveBtn() noexcept
{
    using State = ConsoleBtn::State;
    bSave.setState(console.isEdited() ? State::Active : State::Disabled);
}

void MFilePage::doSave() noexcept
{
    if (!console.isEdited()) {
        return;
    }

    if (console.saveShow()) {
        ui().showFileMsg(Msg::AfterSaveOk, uiMsg::kFileSaved);
        return;
    }

    switch (console.getStatus()) {
    case MConsole::Status::NoShowOpen:
        ui().showFileMsg(Msg::GoSaveAs, MConsole::statusText(MConsole::Status::NoShowOpen));
        break;
    default:
        ui().showConsoleStatus(Msg::None);
        break;
    }
}

void MFilePage::beginNew() noexcept
{
    if (console.isEdited()) {
        ui().showFileYesNo(Msg::ConfirmNew, uiMsg::kConfirmNewShowDiscard);
        return;
    }
    ui().showFileYesNo(Msg::ConfirmNew, uiMsg::kConfirmNewShow);
}

void MFilePage::commitNew() noexcept
{
    console.newShow();
    ui().switchPage(ui().work);
}

void MFilePage::onTouch(const nex::msg::evTouch& e)
{
    if (e.state != nex::TouchState::Release) {
        return;
    }

    using PM = nex::hmi::Page_mFile;
    switch (e.route.comp) {
    case PM::bSave:
        if (bSave.getState() == ConsoleBtn::State::Disabled) {
            return;
        }
        doSave();
        break;
    case PM::bNew:
        beginNew();
        break;
    case PM::bSettings:
        ui().switchPage(ui().settings);
        break;
    default:
        break;
    }
}

void MFilePage::onMsgBox(const nex::msg::evMsgBox& e)
{
    switch (static_cast<Msg>(e.tag)) {
    case Msg::GoSaveAs:
        if (e.action == nex::msg::evMsgBox::Action::Ok) {
            ui().browser.enterSaveAs();
        }
        break;

    case Msg::ConfirmNew:
        if (e.action == nex::msg::evMsgBox::Action::Yes) {
            commitNew();
        }
        break;

    case Msg::AfterSaveOk:
        ui().switchPage(ui().work);
        break;

    case Msg::None:
    default:
        break;
    }
}

} // namespace server
