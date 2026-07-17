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
    if (console.saveShow(console.showName())) {
        ui().showFileMsg(Msg::AfterSaveOk, "File saved.");
        return;
    }

    switch (console.getStatus()) {
    case MConsole::Status::NoShowOpen:
        ui().showFileMsg(Msg::GoSaveAs, "No file open. Use Save As.");
        break;
    case MConsole::Status::TemplateProtected:
        ui().showFileMsg(Msg::None, "Cannot save template.");
        break;
    case MConsole::Status::IoError:
    case MConsole::Status::Ok:
    default:
        ui().showFsError();
        break;
    }
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
       ui().showFileYesNo(Msg::ConfirmNew, "Create new show?");
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
            console.newShow();
            ui().switchPage(ui().work);
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
