#include "mFilePage.hpp"

#include "application.hpp"

#include <cstring>

namespace server {

namespace {

[[nodiscard]] const char* showBaseName(const char* path) noexcept
{
    if (path == nullptr || path[0] == '\0') {
        return "";
    }
    const char* base = path;
    for (const char* p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    return base;
}

[[nodiscard]] bool isTemplateName(const char* name) noexcept
{
    static constexpr char kTemplate[] = {
        static_cast<char>(0xFB), static_cast<char>(0xE1), static_cast<char>(0xE2),
        static_cast<char>(0xEC), static_cast<char>(0xEF), static_cast<char>(0xEE),
        '\0',
    };
    return std::strncmp(name, kTemplate, sizeof(kTemplate) - 1u) == 0;
}

} // namespace

MFilePage::MFilePage(nex::IAppUI& app) noexcept
    : Page<9>(app, HMI_COMP_OBJNAME(mFile), PG::kPageId)
{}

Application& MFilePage::ui() const noexcept
{
    return static_cast<Application&>(app);
}

void MFilePage::showFileMsg(uint8_t tag, const char* text) noexcept
{
    ui().msgBox.setRoute(nex::Route{PG::kPageId, 0u});
    ui().msgBox.show("File", nex::ovl::MsgBox::Preset::OK, tag,
        nex::ovl::MsgBox::Action::Ok, "%s", text);
}

void MFilePage::showFileYesNo(uint8_t tag, const char* text) noexcept
{
    ui().msgBox.setRoute(nex::Route{PG::kPageId, 0u});
    ui().msgBox.show("File", nex::ovl::MsgBox::Preset::YesNo, tag,
        nex::ovl::MsgBox::Action::No, "%s", text);
}

void MFilePage::showFsError() noexcept
{
    _msg = Msg::None;
    showFileMsg(0u, "Storage error.");
}

void MFilePage::finishToWork() noexcept
{
    _msg = Msg::None;
    ui().switchPage(ui().work);
}

void MFilePage::goSaveAs() noexcept
{
    _msg = Msg::None;
    ui().browser.enterSaveAs();
}

void MFilePage::doSave() noexcept
{
    const char* const path = console.showName();
    if (path == nullptr || path[0] == '\0') {
        _msg = Msg::GoSaveAs;
        showFileMsg(kTagGoSaveAs, "No file open. Use Save As.");
        return;
    }

    const char* const base = showBaseName(path);
    if (isTemplateName(base)) {
        _msg = Msg::None;
        showFileMsg(0u, "Cannot save template.");
        return;
    }

    if (!console.saveShow(path)) {
        showFsError();
        return;
    }

    _msg = Msg::AfterSaveOk;
    showFileMsg(kTagAfterSave, "File saved.");
}

void MFilePage::beginNew() noexcept
{
    _msg = Msg::ConfirmNew;
    showFileYesNo(kTagConfirmNew, "Create new show?");
}

void MFilePage::commitNew() noexcept
{
    console.newShow();
    finishToWork();
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
    const Msg msg = _msg;
    _msg = Msg::None;

    switch (msg) {
    case Msg::GoSaveAs:
        if (e.tag == kTagGoSaveAs && e.action == nex::msg::evMsgBox::Action::Ok) {
            goSaveAs();
            return;
        }
        break;

    case Msg::ConfirmNew:
        if (e.tag == kTagConfirmNew && e.action == nex::msg::evMsgBox::Action::Yes) {
            commitNew();
            return;
        }
        break;

    case Msg::AfterSaveOk:
        if (e.tag == kTagAfterSave) {
            finishToWork();
            return;
        }
        break;

    case Msg::None:
    default:
        break;
    }
}

} // namespace server
