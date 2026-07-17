#include "mFilePage.hpp"

#include "application.hpp"

namespace server {

MFilePage::MFilePage(nex::IAppUI& app) noexcept
    : Page<9>(app, HMI_COMP_OBJNAME(mFile), PG::kPageId)
{}

Application& MFilePage::ui() const noexcept
{
    return static_cast<Application&>(app);
}

void MFilePage::doSave() noexcept
{
    if (console.saveShow(mBrowser, console.showName())) {
        ui().showFileMsg(Msg::AfterSaveOk, "File saved.");
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
        ui().showFileYesNo(Msg::ConfirmNew, "Show not saved.\nCreate new anyway?");
        return;
    }
    ui().showFileYesNo(Msg::ConfirmNew, "Create new show?");
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
        doSave();
        break;
    case PM::bNew:
        beginNew();
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
