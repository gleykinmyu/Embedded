#include "mGroupPage.hpp"

#include "application.hpp"

namespace server {

MGroupPage::MGroupPage(nex::IAppUI& app) noexcept
    : Page<7>(app, HMI_COMP_OBJNAME(mGroup), PageCfg::kPageId)
{}

Application& MGroupPage::ui() const noexcept
{
    return static_cast<Application&>(app);
}

uint8_t MGroupPage::resolveGroupId() const noexcept
{
    const int32_t slot = gbid.val;
    if (slot < 0 || slot >= static_cast<int32_t>(WorkPage::GroupButtons::kCount)) {
        return MConsole::kBlockedGroupId;
    }
    return ui().work.groupIdForSlot(static_cast<uint8_t>(slot));
}

void MGroupPage::requestGroupId(Action action) noexcept
{
    _action = action;
    _groupId = 0xFFu;
    gbid.val.get();
}

void MGroupPage::onTouch(const nex::msg::evTouch& e)
{
    if (e.state != nex::TouchState::Release) {
        return;
    }

    using PM = nex::hmi::Page_mGroup;
    switch (e.route.comp) {
    case PM::bRec:
        requestGroupId(Action::Record);
        break;
    case PM::bClr:
        requestGroupId(Action::Clear);
        break;
    case PM::bRen:
        requestGroupId(Action::Rename);
        break;
    default:
        break;
    }
}

void MGroupPage::onResponse(const nex::msg::getNumeric& response, nex::Route route, uint8_t tag)
{
    Page::onResponse(response, route, tag);

    if (_action == Action::None) {
        return;
    }
    if (route.comp != gbid.id()) {
        return;
    }

    const uint8_t group_id = resolveGroupId();
    if (group_id == MConsole::kBlockedGroupId) {
        _action = Action::None;
        return;
    }

    handleAction(group_id);
}

void MGroupPage::handleAction(uint8_t group_id) noexcept
{
    const Action action = _action;
    _groupId = group_id;

    switch (action) {
    case Action::Record:
        if (console.group(group_id).isEmpty()) {
            doRecord(group_id);
        } else {
            ui().msgBox.setRoute(nex::Route{PageCfg::kPageId, 0u});
            ui().msgBox.show("Group", nex::ovl::MsgBox::Preset::YesNo, kTagOverwrite,
                             nex::ovl::MsgBox::Action::No, "Overwrite group?");
        }
        break;

    case Action::Clear:
        if (console.group(group_id).isEmpty()) {
            _action = Action::None;
            finishToWork();
        } else {
            ui().msgBox.setRoute(nex::Route{PageCfg::kPageId, 0u});
            ui().msgBox.show("Group", nex::ovl::MsgBox::Preset::YesNo, kTagClear,
                             nex::ovl::MsgBox::Action::No, "Clear group?");
        }
        break;

    case Action::Rename:
        doRename(group_id);
        break;

    case Action::None:
    default:
        break;
    }
}

void MGroupPage::doRecord(uint8_t group_id) noexcept
{
    _action = Action::None;
    (void)console.createGroup(group_id);
    finishToWork();
}

void MGroupPage::doClear(uint8_t group_id) noexcept
{
    _action = Action::None;
    (void)console.clearGroup(group_id);
    finishToWork();
}

void MGroupPage::doRename(uint8_t group_id) noexcept
{
    _action = Action::None;

    const smcp::Group& grp = console.group(group_id);
    if (grp.isEmpty()) {
        finishToWork();
        return;
    }

    ui().work.gName.txt.set(grp.name);
    ui().work.beginRename(group_id);
    ui().showKeybdFull(ui().work.gName);
}

void MGroupPage::finishToWork() noexcept
{
    _action = Action::None;
    _groupId = 0xFFu;
    // switchPage(name) держит указатель на Literal — временный от objname небезопасен.
    ui().switchPage(ui().work);
}

void MGroupPage::onMsgBox(const nex::msg::evMsgBox& e)
{
    if (e.action != nex::msg::evMsgBox::Action::Yes) {
        _action = Action::None;
        _groupId = 0xFFu;
        finishToWork();
        return;
    }

    const uint8_t group_id = _groupId;
    if (group_id == 0xFFu) {
        _action = Action::None;
        return;
    }

    if (e.tag == kTagOverwrite) {
        doRecord(group_id);
    } else if (e.tag == kTagClear) {
        doClear(group_id);
    } else {
        _action = Action::None;
        finishToWork();
    }
}

} // namespace server
