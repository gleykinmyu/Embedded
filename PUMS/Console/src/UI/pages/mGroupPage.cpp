#include "mGroupPage.hpp"

#include "UI/application.hpp"
#include "UI/uiMessages.hpp"

namespace server {

MGroupPage::MGroupPage(nex::IAppUI& app) noexcept
    : Page<3>(app, HMI_COMP_OBJNAME(mGroup), PG::kPageId)
{}

Application& MGroupPage::ui() const noexcept
{
    return static_cast<Application&>(app);
}

void MGroupPage::setAssignSlot(uint8_t slot) noexcept
{
    _assignSlot = slot;
    _action = Action::None;
    _groupId = 0xFFu;
}

void MGroupPage::refreshActionButtons() noexcept
{
    using State = ConsoleBtn::State;

    const bool canRecord = console.selectedCount() > 0u;
    bRec.setState(canRecord ? State::Active : State::Disabled);

    const uint8_t group_id = resolveGroupId();
    const bool groupEmpty = (group_id == MConsole::kBlockedGroupId)
        || console.group(group_id).isEmpty();
    const State editState = groupEmpty ? State::Disabled : State::Active;
    bRen.setState(editState);
    bClr.setState(editState);
}

void MGroupPage::onLoad()
{
    refreshActionButtons();
}

uint8_t MGroupPage::resolveGroupId() const noexcept
{
    if (_assignSlot >= WorkPage::GroupButtons::kCount) {
        return MConsole::kBlockedGroupId;
    }
    return ui().work.groupIdForSlot(_assignSlot);
}

void MGroupPage::runAction(Action action) noexcept
{
    _action = action;
    _groupId = 0xFFu;

    const uint8_t group_id = resolveGroupId();
    if (group_id == MConsole::kBlockedGroupId) {
        _action = Action::None;
        return;
    }

    handleAction(group_id);
}

void MGroupPage::onTouch(const nex::msg::evTouch& e)
{
    if (e.state != nex::TouchState::Release) {
        return;
    }

    using PM = nex::hmi::Page_mGroup;
    using State = ConsoleBtn::State;

    switch (e.route.comp) {
    case PM::bRec:
        if (bRec.getState() == State::Disabled) {
            return;
        }
        runAction(Action::Record);
        break;
    case PM::bClr:
        if (bClr.getState() == State::Disabled) {
            return;
        }
        runAction(Action::Clear);
        break;
    case PM::bRen:
        if (bRen.getState() == State::Disabled) {
            return;
        }
        runAction(Action::Rename);
        break;
    default:
        break;
    }
}

void MGroupPage::handleAction(uint8_t group_id) noexcept
{
    const Action action = _action;
    _groupId = group_id;

    switch (action) {
    case Action::Record:
        if (!console.recordGroup(group_id)) {
            if (console.getStatus() == MConsole::Status::GroupOccupied) {
                ui().showGroupYesNo(kTagOverwrite, uiMsg::kConfirmOverwriteGroup);
                return;
            }
            // NoSelection («Сначала выберите лебёдки») и прочие ошибки —
            // MsgBox, на work уходим после OK/Cancel в onMsgBox.
            ui().showGroupStatus();
            return;
        }
        finishToWork();
        break;

    case Action::Clear:
        if (!console.clearGroup(group_id)) {
            if (console.getStatus() == MConsole::Status::GroupOccupied) {
                ui().showGroupYesNo(kTagClear, uiMsg::kConfirmClearGroup);
                return;
            }
            ui().showGroupStatus();
            return;
        }
        finishToWork();
        break;

    case Action::Rename:
        doRename(group_id);
        break;

    case Action::None:
    default:
        break;
    }
}

void MGroupPage::doRename(uint8_t group_id) noexcept
{
    _action = Action::None;

    const smcp::Group& grp = console.group(group_id);
    if (grp.isEmpty()) {
        finishToWork();
        return;
    }

    ui().work.gName.txt.setGlobal(grp.name);
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
    // Yes — только подтверждение перезаписи/очистки; Ok/No/Cancel → на work.
    if (e.action != nex::msg::evMsgBox::Action::Yes) {
        _action = Action::None;
        _groupId = 0xFFu;
        finishToWork();
        return;
    }

    const uint8_t group_id = _groupId;
    if (group_id == 0xFFu) {
        _action = Action::None;
        finishToWork();
        return;
    }

    if (e.tag == kTagOverwrite) {
        if (!console.recordGroup(group_id, nullptr, true)) {
            // Например NoSelection — показать и уйти после следующего OK.
            ui().showGroupStatus();
            return;
        }
    } else if (e.tag == kTagClear) {
        if (!console.clearGroup(group_id, true)) {
            ui().showGroupStatus();
            return;
        }
    }
    finishToWork();
}

} // namespace server
