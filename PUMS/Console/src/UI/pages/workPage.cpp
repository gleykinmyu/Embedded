#include "workPage.hpp"

#include "UI/application.hpp"
#include "UI/uiMessages.hpp"

#include <cstdio>

namespace server {

namespace {

constexpr uint8_t kScenePageCount = 4u;

smcp::IGroupBank::OverlapSlot overlapSlots[smcp::kGroupMaxCount]{};

[[nodiscard]] uint8_t shownGroup() noexcept
{
    const uint8_t queued = console.queuedGroup();
    return (queued != smcp::IGroupConsole::kNoQueuedGroup) ? queued : console.activeGroup();
}

[[nodiscard]] bool isMechBlocked(uint8_t id) noexcept
{
    return console.cmechs[id].isBlocked() || console.show.group.containsBlocked(id);
}

[[nodiscard]] bool isMechIsolated(uint8_t id) noexcept
{
    if (!console.show.settings().isolateGroup || id >= MConsole::kMechCount) {
        return false;
    }
    const uint8_t active = console.activeGroup();
    if (active == MConsole::kNoActiveGroup) {
        return false;
    }
    return !console.show.group[active].mech().contains(id);
}

[[nodiscard]] bool formatIdList(char* out, std::size_t outLen, const uint8_t* ids, uint8_t count,
                                uint8_t displayBias = 0u, const char* mark = nullptr) noexcept
{
    if (out == nullptr || outLen == 0u) {
        return false;
    }
    out[0] = '\0';
    if (ids == nullptr || count == 0u) {
        return false;
    }
    const char* const prefix = (mark != nullptr) ? mark : "";
    std::size_t pos = 0u;
    for (uint8_t i = 0; i < count; ++i) {
        const unsigned shown = static_cast<unsigned>(ids[i]) + static_cast<unsigned>(displayBias);
        const int n = (pos == 0u)
            ? std::snprintf(out + pos, outLen - pos, "%s%u", prefix, shown)
            : std::snprintf(out + pos, outLen - pos, ", %s%u", prefix, shown);
        if (n <= 0 || static_cast<std::size_t>(n) >= outLen - pos) {
            out[outLen - 1u] = '\0';
            return pos > 0u;
        }
        pos += static_cast<std::size_t>(n);
    }
    return true;
}

bool fillMechBlockMessage(uint8_t id, char* out, std::size_t outLen) noexcept
{
    if (out == nullptr || outLen == 0u || !isMechBlocked(id)) {
        return false;
    }
    const unsigned winchNo = static_cast<unsigned>(id) + 1u;
    if (console.cmechs[id].isBlocked()) {
        std::snprintf(out, outLen, uiMsg::kBlockByServerFmt, uiMsg::kWinchMark, winchNo);
        return true;
    }
    smcp::Selection one;
    one.add(id);
    if (!console.show.group.fillBlockedOverlap(smcp::IGroupBank::kNoExcept, one)) {
        return false;
    }
    const auto& ov = console.show.group.overlap();
    if (ov.slot == nullptr || ov.count == 0u) {
        return false;
    }
    uint8_t groupIds[smcp::kGroupMaxCount]{};
    for (uint8_t i = 0; i < ov.count; ++i) {
        groupIds[i] = ov.slot[i].group;
    }
    char list[96]{};
    (void)formatIdList(list, sizeof(list), groupIds, ov.count);
    std::snprintf(out, outLen, uiMsg::kBlockByGroupsFmt, uiMsg::kWinchMark, winchNo, list);
    return true;
}

void deselectGroupMechs(uint8_t group_id) noexcept
{
    const smcp::Selection& sel = console.show.group[group_id].mech();
    for (uint8_t m = 0; m < MConsole::kMechCount; ++m) {
        if (!sel.contains(m)) {
            continue;
        }
        if (console.cmechs[m].isSelectedBy(console.consoleId())) {
            console.cmechs[m].select(smcp::kHolderNone);
        }
    }
    if (console.activeGroup() == group_id) {
        console.clearActiveGroup();
    }
}

bool pressMechSelect(uint8_t id) noexcept
{
    if (id >= MConsole::kMechCount || isMechBlocked(id)) {
        return false;
    }
    smcp::CGMech& m = console.cmechs[id];
    if (m.isSelectedBy(console.consoleId())) {
        m.select(smcp::kHolderNone);
        return true;
    }
    if (m.isSelected() || isMechIsolated(id)) {
        return false;
    }
    return m.select(console.consoleId());
}

} // namespace

WorkPage::WorkPage(nex::IAppUI& app) noexcept
    : Page<54>(app, HMI_COMP_OBJNAME(work), PG::kPageId)
{}

uint8_t WorkPage::groupIdForSlot(uint8_t index) const noexcept
{
    return static_cast<uint8_t>(static_cast<uint8_t>(_scenePage * GroupButtons::kCount) + index);
}

void WorkPage::beginRename(uint8_t group_id) noexcept
{
    _renameGroupId = group_id;
}

void WorkPage::onLoad()
{
    console.show.group.setOverlapArray(overlapSlots, smcp::kGroupMaxCount);
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

    (void)console.show.group[group_id].rename(*gName.txt);
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

    if (_blockOn) {
        if (state != nex::TouchState::Release || index >= MConsole::kMechCount) {
            return;
        }
        smcp::CGMech& m = console.cmechs[index];
        m.block(!m.isBlocked());
        refreshGroupBtn(false);
        refreshCells();
        return;
    }

    /* Work: заблокированная лебёдка — MsgBox на Release. */
    if (console.mode() == MConsole::Mode::Work && isMechBlocked(index)) {
        char msg[160]{};
        if (state == nex::TouchState::Release && fillMechBlockMessage(index, msg, sizeof(msg))) {
            showBlockMsg(msg);
        }
        return;
    }

    if (state != nex::TouchState::Press) {
        return;
    }

    if (pressMechSelect(index)) {
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
    const auto grp = console.show.group[group_id];
    if (grp.isEmpty()) {
        return;
    }

    if (_blockOn) {
        if (state != nex::TouchState::Release) {
            return;
        }
        if (grp.isBlocked()) {
            console.show.group[group_id].setBlocked(false);
            refreshGroupBtn(false);
            refreshCells();
            return;
        }

        console.show.group[group_id].setBlocked(true);
        deselectGroupMechs(group_id);
        refreshGroupBtn(false);
        refreshCells();

        const auto& ov = console.show.group.overlap();
        if (ov.count == 0u) {
            return;
        }
        uint8_t sharedIds[MConsole::kMechCount]{};
        uint8_t sharedCount = 0u;
        for (uint8_t m = 0; m < MConsole::kMechCount; ++m) {
            if (ov.mechs().contains(m)) {
                sharedIds[sharedCount++] = m;
            }
        }
        char list[96]{};
        char msg[160]{};
        (void)formatIdList(list, sizeof(list), sharedIds, sharedCount, 1u, uiMsg::kWinchMark);
        std::snprintf(msg, sizeof(msg), uiMsg::kBlockSharedWinchesFmt, list);
        showBlockMsg(msg);
        return;
    }

    if (grp.isBlocked()) {
        return;
    }

    if (state == nex::TouchState::Press) {
        const bool deselect = (group_id == shownGroup());
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

    if (group_id == shownGroup()) {
        console.clearActiveGroup();
    } else {
        switch (console.show.group[group_id].recall()) {
        case smcp::CGroup::Result::Ok:
            break;
        case smcp::CGroup::Result::OverlapsBlocked: {
            const auto& ov = console.show.group.overlap();
            uint8_t sharedIds[MConsole::kMechCount]{};
            uint8_t sharedCount = 0u;
            for (uint8_t m = 0; m < MConsole::kMechCount; ++m) {
                if (ov.mechs().contains(m)) {
                    sharedIds[sharedCount++] = m;
                }
            }
            char list[96]{};
            char msg[160]{};
            (void)formatIdList(list, sizeof(list), sharedIds, sharedCount, 1u, uiMsg::kWinchMark);
            std::snprintf(msg, sizeof(msg), uiMsg::kBlockSharedWinchesFmt, list);
            showBlockMsg(msg);
            return;
        }
        case smcp::CGroup::Result::Empty:
        case smcp::CGroup::Result::Blocked:
        case smcp::CGroup::Result::Occupied:
        default:
            return;
        }
    }

    /* Ячейки: isolate на Ack Select, Selected — с Telemetry. */
    refreshGroupBtn(false);
}

void WorkPage::onAssignPress(uint8_t index) noexcept
{
    if (index >= GroupAssignButtons::kCount) {
        return;
    }
    if (groupAssignBtn[index].getState() == ConsoleBtn::State::Disabled) {
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
        if (console.mode() != MConsole::Mode::Work || _blockOn) {
            return;
        }
        ui().switchPage(ui().mFile);
        break;

    case PW::bBlock:
        if (console.mode() == MConsole::Mode::Show) {
            return;
        }
        _blockOn = !_blockOn;
        applyModeChange();
        break;

    case PW::bShow:
        if (console.mode() == MConsole::Mode::Show) {
            showExitShowConfirm();
            return;
        }
        _blockOn = false;
        console.setMode(MConsole::Mode::Show);
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
    /* ExitShow Yes — режим; redraw кнопок в onAfterMsgBox (после refreshPage). */
    if (e.tag != kTagExitShow) {
        return;
    }
    if (e.action != nex::msg::evMsgBox::Action::Yes) {
        return;
    }
    console.setMode(MConsole::Mode::Work);
}

void WorkPage::onAfterMsgBox(const nex::msg::evMsgBox& e)
{
    if (e.tag == kTagBlockMsg) {
        refreshGroupBtn(false);
        refreshCells();
        return;
    }
    if (e.tag == kTagExitShow && e.action == nex::msg::evMsgBox::Action::Yes) {
        applyModeChange();
    }
}

Application& WorkPage::ui() const noexcept
{
    return static_cast<Application&>(app);
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

    const State fileState = (mode == MConsole::Mode::Work && !_blockOn) ? State::Active : State::Disabled;
    if (bFile.getState() != fileState) {
        bFile.setState(fileState);
    }

    State blockState = State::Active;
    if (mode == MConsole::Mode::Show) {
        blockState = State::Disabled;
    } else if (_blockOn) {
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
    const uint8_t active_id = shownGroup();

    for (uint8_t i = 0; i < GroupButtons::kCount; ++i) {
        const uint8_t group_id = groupIdForSlot(i);
        const auto grp = console.show.group[group_id];

        if (textModified) {
            char preamble[] = "  00 - ";
            preamble[2] = static_cast<char>('0' + (group_id / 10u));
            preamble[3] = static_cast<char>('0' + (group_id % 10u));
            groupBtn[i].txt.set(preamble);
            groupBtn[i].appendText(grp.name());
        }

        State next = State::Disabled;
        if (grp.isBlocked()) {
            next = State::GroupBlocked;
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
    const bool assignOk = (console.mode() == MConsole::Mode::Work && !_blockOn);
    const bool hasSelection = console.hasSelection();
    const uint8_t active_id = shownGroup();

    for (uint8_t i = 0; i < GroupAssignButtons::kCount; ++i) {
        const uint8_t group_id = groupIdForSlot(i);
        const auto grp = console.show.group[group_id];

        State next = State::Disabled;
        if (assignOk) {
            if (grp.isEmpty()) {
                if (hasSelection) {
                    next = State::Active;
                }
            } else if (grp.isBlocked()) {
                next = State::GroupBlocked;
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
    /* Серверный Block — поверх GRUP; isolate — поверх Select (чужие скрываем).
     * Нет Ready (Idle) → Disabled — отдельного NotReady в UI нет. */
    if (console.cmechs[index].isBlocked()) {
        next = State::Blocked;
    } else if (console.show.group.containsBlocked(index)) {
        next = State::GroupBlocked;
    } else if (isMechIsolated(index)) {
        next = State::Disabled;
    } else if (console.cmechs[index].isSelectedBy(console.consoleId())
               && console.cmechs[index].status().any(smcp::IMech::Status::Ready)) {
        next = State::Selected;
    } else if (console.cmechs[index].status().any(smcp::IMech::Status::Ready)) {
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

void WorkPage::onSelectAck() noexcept
{
    /* Ack Select: commit active уже в модели; чужие → Disabled; Selected — Telemetry. */
    refreshGroupBtn(false);
    refreshCells();
}

void WorkPage::onSelectNack() noexcept
{
    /* queued сброшен; active прежний — вернуть кнопки групп / ячейки. */
    refreshGroupBtn(false);
    refreshCells();
}

} // namespace server
