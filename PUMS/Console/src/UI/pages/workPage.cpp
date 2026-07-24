#include "workPage.hpp"

#include "UI/application.hpp"
#include "UI/uiMessages.hpp"

namespace server {

namespace {

constexpr uint8_t kScenePageCount = 4u;

} // namespace

WorkPage::WorkPage(nex::IAppUI& app) noexcept
    : Page<54>(app, HMI_COMP_OBJNAME(work), PG::kPageId)
{}

uint8_t WorkPage::groupIdForSlot(uint8_t index) const noexcept
{
    return static_cast<uint8_t>(1u + static_cast<uint8_t>(_scenePage * GroupButtons::kCount) + index);
}

void WorkPage::beginRename(uint8_t group_id) noexcept
{
    _renameGroupId = group_id;
}

void WorkPage::onLoad()
{
    refreshModeButtons();
    refreshGroupBtn(true);
    refreshCells();

    if (_renameGroupId != 0xFFu) {
        gName.txt.get();
    }
}

void WorkPage::onResponse(const nex::msg::getString& response, nex::Route route, uint8_t tag)
{
    Page::onResponse(response, route, tag);

    if (_renameGroupId == 0xFFu) {
        return;
    }
    if (route.comp != gName.id()) {
        return;
    }

    finishRename();
}

void WorkPage::finishRename() noexcept
{
    const uint8_t group_id = _renameGroupId;
    _renameGroupId = 0xFFu;

    if (group_id == 0xFFu) {
        return;
    }

    (void)console.renameGroup(group_id, *gName.txt);
    refreshGroupBtn(true);
}

void WorkPage::onTouch(const nex::msg::evTouch& e)
{
    using PW = nex::hmi::Page_work;
    const uint8_t comp = e.route.comp;

    if (comp == PW::bFile || comp == PW::bBlock || comp == PW::bShow) {
        if (e.state == nex::TouchState::Release) {
            onMenuPress(comp);
        }
        return;
    }

    if (comp >= PW::bSa0 && comp <= PW::bSa7) {
        if (e.state == nex::TouchState::Release) {
            onAssignPress(static_cast<uint8_t>(comp - PW::bSa0));
        }
        return;
    }

    if (comp == PW::bSNext || comp == PW::bSPrev
        || (comp >= PW::bS0 && comp <= PW::bS7)) {
        onGroupPress(comp, e.state);
        return;
    }

    const uint8_t cellFirst = PW::b0;
    const uint8_t cellLast = PW::b23;
    if (comp >= cellFirst && comp <= cellLast) {
        onCellPress(static_cast<uint8_t>(comp - cellFirst), e.state);
    }
}

void WorkPage::onCellPress(uint8_t index, nex::TouchState state)
{
    if (index >= CellButtons::kCount) {
        return;
    }

    if (console.mode() == MConsole::Mode::Block) {
        if (state == nex::TouchState::Release) {
            applyBlockResult(console.pressMech(index));
        }
        return;
    }

    /* Work: заблокированная лебёдка — MsgBox на Release. */
    if (console.mode() == MConsole::Mode::Work && console.isMechBlocked(index)) {
        if (state == nex::TouchState::Release && console.fillMechBlockMessage(index)) {
            showBlockMsg(console.blockMessage());
        }
        return;
    }

    if (state != nex::TouchState::Press) {
        return;
    }

    const MConsole::BlockResult result = console.pressMech(index);
    if (result == MConsole::BlockResult::Changed) {
        refreshCell(index);
        refreshAssignBtn();
    }
}

void WorkPage::onGroupPress(uint8_t comp, nex::TouchState state)
{
    using PW = nex::hmi::Page_work;
    using State = ConsoleBtn::State;

    if (comp == PW::bSNext || comp == PW::bSPrev) {
        if (state != nex::TouchState::Release) {
            return;
        }

        const uint8_t delta = (comp == PW::bSNext) ? 1u : kScenePageCount - 1u;
        _scenePage = static_cast<uint8_t>((_scenePage + delta) % kScenePageCount);

        static constexpr const char* kPageLabels[] = {"1/4", "2/4", "3/4", "4/4"};
        const char* label = kPageLabels[0];
        if (_scenePage < kScenePageCount) {
            label = kPageLabels[_scenePage];
        }
        tgPage.setText(label);
        refreshGroupBtn(true);
        return;
    }

    if (comp < PW::bS0 || comp > PW::bS7) {
        return;
    }

    const uint8_t index = static_cast<uint8_t>(comp - PW::bS0);
    if (index >= GroupButtons::kCount) {
        return;
    }

    const uint8_t group_id = groupIdForSlot(index);
    const smcp::Group& grp = console.group(group_id);
    if (grp.isEmpty()) {
        return;
    }

    if (console.mode() == MConsole::Mode::Block) {
        if (state == nex::TouchState::Release) {
            applyBlockResult(console.pressGroup(group_id));
        }
        return;
    }

    if (grp.isBlocked()) {
        return;
    }

    if (state == nex::TouchState::Press) {
        const bool deselect = (group_id == console.getActiveGroup());
        const State self = deselect ? State::Active : State::Selected;

        if (groupBtn[index].getState() != self) {
            groupBtn[index].setState(self);
        }
        if (groupAssignBtn[index].getState() != self) {
            groupAssignBtn[index].setState(self);
        }
        return;
    }

    if (state != nex::TouchState::Release) {
        return;
    }

    if (console.pressGroup(group_id) == MConsole::BlockResult::NoChange) {
        return;
    }

    refreshGroupBtn(false);
    refreshCells();
}

void WorkPage::onAssignPress(uint8_t index) noexcept
{
    if (index >= GroupAssignButtons::kCount) {
        return;
    }
    if (groupAssignBtn[index].getState() == ConsoleBtn::State::Disabled) {
        return;
    }

    const uint8_t group_id = groupIdForSlot(index);
    if (group_id == MConsole::kBlockedGroupId) {
        return;
    }

    ui().mGroup.setAssignSlot(index);
    ui().switchPage(ui().mGroup);
}

void WorkPage::onMenuPress(uint8_t comp)
{
    using PW = nex::hmi::Page_work;

    switch (comp) {
    case PW::bFile:
        if (!console.allowsFileMenu()) {
            return;
        }
        ui().switchPage(ui().mFile);
        break;

    case PW::bBlock:
        if (!console.allowsBlockMode()) {
            return;
        }
        (void)console.toggleMode(MConsole::Mode::Block);
        applyModeChange();
        break;

    case PW::bShow:
        if (console.mode() == MConsole::Mode::Show) {
            showExitShowConfirm();
            return;
        }
        (void)console.toggleMode(MConsole::Mode::Show);
        applyModeChange();
        break;

    default:
        break;
    }
}

void WorkPage::applyModeChange() noexcept
{
    refreshModeButtons();
    refreshGroupBtn(false);
    refreshCells();
}

void WorkPage::showExitShowConfirm() noexcept
{
    ui().showUtf8Msg(uiMsg::kTitleShow, nex::ovl::MsgBox::Preset::YesNo, kTagExitShow,
        nex::ovl::MsgBox::Action::No, uiMsg::kConfirmExitShow);
}

void WorkPage::onMsgBox(const nex::msg::evMsgBox& e)
{
    if (e.tag == kTagBlockMsg) {
        /* После refreshPage из MsgBox::onClick — вернуть цвета кнопок. */
        refreshGroupBtn(false);
        refreshCells();
        return;
    }
    if (e.tag != kTagExitShow) {
        return;
    }
    if (e.action != nex::msg::evMsgBox::Action::Yes) {
        return;
    }
    console.setMode(MConsole::Mode::Work);
    applyModeChange();
}

Application& WorkPage::ui() const noexcept
{
    return static_cast<Application&>(app);
}

void WorkPage::applyBlockResult(MConsole::BlockResult result) noexcept
{
    const bool modelChanged = (result == MConsole::BlockResult::Changed
        || result == MConsole::BlockResult::Warning);

    if (result == MConsole::BlockResult::Rejected
        || result == MConsole::BlockResult::Warning) {
        const char* msg = console.blockMessage();
        if (msg != nullptr && msg[0] != '\0') {
            /* Без redraw до диалога — иначе setState рисует поверх canvas MsgBox. */
            showBlockMsg(msg);
            return;
        }
    }

    if (modelChanged) {
        refreshGroupBtn(false);
        refreshCells();
    }
}

void WorkPage::showBlockMsg(const char* text) noexcept
{
    /* text — UTF-8 литерал из прошивки (не имя с SD). */
    ui().showUtf8Msg(uiMsg::kTitleBlock, nex::ovl::MsgBox::Preset::OK, kTagBlockMsg,
        nex::ovl::MsgBox::Action::Ok, text);
}

void WorkPage::refreshModeButtons() noexcept
{
    using State = ConsoleBtn::State;
    const MConsole::Mode mode = console.mode();

    const State fileState = console.allowsFileMenu() ? State::Active : State::Disabled;
    if (bFile.getState() != fileState) {
        bFile.setState(fileState);
    }

    State blockState = State::Active;
    if (!console.allowsBlockMode()) {
        blockState = State::Disabled;
    } else if (mode == MConsole::Mode::Block) {
        blockState = State::Selected;
    }
    if (bBlock.getState() != blockState) {
        bBlock.setState(blockState);
    }

    const State showState = (mode == MConsole::Mode::Show) ? State::Selected : State::Active;
    if (bShow.getState() != showState) {
        bShow.setState(showState);
    }
}

void WorkPage::refreshGroupBtn(bool textModified) noexcept
{
    using State = ConsoleBtn::State;
    const uint8_t active_id = console.getActiveGroup();

    for (uint8_t i = 0; i < GroupButtons::kCount; ++i) {
        const uint8_t group_id = groupIdForSlot(i);
        const smcp::Group& grp = console.group(group_id);

        if (textModified) {
            char preamble[] = "  00 - ";
            preamble[2] = static_cast<char>('0' + (group_id / 10u));
            preamble[3] = static_cast<char>('0' + (group_id % 10u));
            groupBtn[i].txt.set(preamble);
            groupBtn[i].appendText(grp.name);
        }

        State next = State::Disabled;
        if (grp.isBlocked()) {
            next = State::Blocked;
        } else if (!grp.isEmpty()) {
            next = (group_id == active_id) ? State::Selected : State::Active;
        }

        if (groupBtn[i].getState() != next) {
            groupBtn[i].setState(next);
        }
    }

    refreshAssignBtn();
}

void WorkPage::refreshAssignBtn() noexcept
{
    using State = ConsoleBtn::State;
    const bool assignOk = console.allowsGroupEdit();
    const bool hasSelection = console.hasSelection();
    const uint8_t active_id = console.getActiveGroup();

    for (uint8_t i = 0; i < GroupAssignButtons::kCount; ++i) {
        const uint8_t group_id = groupIdForSlot(i);
        const smcp::Group& grp = console.group(group_id);

        State next = State::Disabled;
        if (assignOk) {
            if (grp.isEmpty()) {
                if (hasSelection) {
                    next = State::Active;
                }
            } else if (grp.isBlocked()) {
                next = State::Blocked;
            } else {
                next = (group_id == active_id) ? State::Selected : State::Active;
            }
        }

        if (groupAssignBtn[i].getState() != next) {
            groupAssignBtn[i].setState(next);
        }
    }
}

void WorkPage::refreshCell(uint8_t index) noexcept
{
    if (index >= CellButtons::kCount) {
        return;
    }

    WinchButton& cell = cells[index];
    using State = ConsoleBtn::State;

    State next = State::Disabled;
    if (console.isMechBlocked(index)) {
        next = State::Blocked;
    } else if (console.mech(index).status().all(smcp::IMech::Status::Selected | smcp::IMech::Status::Ready)) {
        next = State::Selected;
    } else if (console.isMechIsolated(index)) {
        next = State::Disabled;
    } else if (console.mech(index).status().any(smcp::IMech::Status::Ready)) {
        next = State::Active;
    }

    if (cell.getState() != next) {
        cell.setState(next);
    }
}

void WorkPage::refreshCells() noexcept
{
    for (uint8_t i = 0; i < CellButtons::kCount; ++i) {
        refreshCell(i);
    }
}

void WorkPage::onMechTelemetry(uint8_t mech_id) noexcept
{
    refreshCell(mech_id);
    refreshAssignBtn();
}

} // namespace server
