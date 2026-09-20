#include "workPage.hpp"

#include "UI/application.hpp"
#include "UI/uiMessages.hpp"

#include <cstdio>

namespace server {

namespace {

smcp::IGroupBank::OverlapSlot overlapSlots[smcp::kGroupMaxCount]{};

[[nodiscard]] uint8_t scenePageCount() noexcept
{
    const uint16_t n = console.show.group.slotCount();
    constexpr uint8_t per = WorkPage::GroupButtons::kCount;
    if (n == 0u) {
        return 1u;
    }
    return static_cast<uint8_t>((n + per - 1u) / per);
}

[[nodiscard]] bool isMechBlocked(uint8_t id) noexcept
{
    return console.cmechs[id].isBlocked() || console.show.group.containsBlocked(id);
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

} // namespace

WorkPage::WorkPage(nex::IAppUI& app) noexcept
    : Page<54>(app, HMI_COMP_OBJNAME(work), PG::kPageId)
{
    console.show.group.setOverlapArray(overlapSlots, smcp::kGroupMaxCount);
}

void WorkPage::onLoad()
{
    refreshMenuButtons();

    if (_renameGroupId != 0xFFu) {
        gName.txt.get();
    }
}

uint8_t WorkPage::groupIdForSlot(uint8_t index) const noexcept
{
    const uint16_t id = static_cast<uint16_t>(_scenePage) * GroupButtons::kCount + index;
    if (id >= console.show.group.slotCount()) {
        return MConsole::kNoActiveGroup;
    }
    return static_cast<uint8_t>(id);
}

void WorkPage::refreshGroupPage() noexcept
{
    const uint8_t pages = scenePageCount();
    if (_scenePage >= pages) {
        _scenePage = static_cast<uint8_t>(pages - 1u);
    }
    char buf[8]{};
    std::snprintf(buf, sizeof(buf), "%u/%u",
        static_cast<unsigned>(_scenePage + 1u),
        static_cast<unsigned>(pages));
    tgPage.txt.set(buf);
}

void WorkPage::beginRename(uint8_t group_id) noexcept
{
    _renameGroupId = group_id;
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
    if (_blockOn) {
        if (state != nex::TouchState::Release) {
            return;
        }
        smcp::CGMech& m = console.cmechs[index];
        m.block(!m.isBlocked());
        return;
    }

    if (state != nex::TouchState::Press || WinchButton::isolated(console, index)) {
        return;
    }

    smcp::CGMech& m = console.cmechs[index];
    const uint8_t want = m.isSelectedBy(console.consoleId())
        ? smcp::kHolderNone
        : console.consoleId();

    using Result = smcp::CGroup::Result;
    const Result r = m.trySelect(want);
    if (r != Result::Blocked && r != Result::OverlapsBlocked) {
        return;
    }
    char msg[160]{};
    if (fillMechBlockMessage(index, msg, sizeof(msg))) {
        showBlockMsg(msg);
    }
}

void WorkPage::onGroupPress(uint8_t comp, nex::TouchState state)
{
    using PW = nex::hmi::Page_work;

    if (comp == PW::bSNext || comp == PW::bSPrev) {
        if (state != nex::TouchState::Release) {
            return;
        }

        const uint8_t pages = scenePageCount();
        const uint8_t delta = (comp == PW::bSNext) ? 1u : static_cast<uint8_t>(pages - 1u);
        _scenePage = static_cast<uint8_t>((_scenePage + delta) % pages);
        refreshGroups(true);
        return;
    }

    const uint8_t index = static_cast<uint8_t>(comp - PW::bS0);
    const uint8_t group_id = groupIdForSlot(index);
    if (group_id == MConsole::kNoActiveGroup) {
        return;
    }
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
            refreshGroups(false);
            refreshCells();
            return;
        }

        console.show.group[group_id].setBlocked(true);
        refreshGroups(false);
        refreshCells();

        const auto& ov = console.show.group.overlap();
        if (ov.count == 0u) {
            return;
        }
        showSharedWinchesMsg();
        return;
    }

    if (state == nex::TouchState::Press) {
        const bool deselect = (group_id == console.shownGroup());
        _recallSlot = index;
        _recallPrevSlot = 0xFFu;
        if (!deselect) {
            const uint8_t prev = console.shownGroup();
            for (uint8_t i = 0; i < GroupButtons::kCount; ++i) {
                if (groupIdForSlot(i) == prev) {
                    _recallPrevSlot = i;
                    break;
                }
            }
            if (_recallPrevSlot < GroupButtons::kCount && _recallPrevSlot != index) {
                paintGroupSlot(_recallPrevSlot, false);
            }
        }
        paintGroupSlot(index, !deselect);
        return;
    }

    if (state != nex::TouchState::Release) {
        return;
    }

    /* Снова, пока Nextion не вернул bco с Press. */
    const bool deselect = (group_id == console.shownGroup());
    paintGroupSlot(index, !deselect);

    if (deselect) {
        console.clearActiveGroup();
        _recallSlot = 0xFFu;
        _recallPrevSlot = 0xFFu;
        return;
    }

    switch (console.show.group[group_id].recall()) {
    case smcp::CGroup::Result::Ok:
        return;
    case smcp::CGroup::Result::OverlapsBlocked:
        showSharedWinchesMsg();
        return;
    case smcp::CGroup::Result::Blocked: {
        char msg[80]{};
        std::snprintf(msg, sizeof(msg), uiMsg::kGroupBlockedFmt,
            static_cast<unsigned>(group_id));
        showBlockMsg(msg);
        return;
    }
    case smcp::CGroup::Result::Empty:
    case smcp::CGroup::Result::Occupied:
    default:
        onSelectNack();
        return;
    }
}

void WorkPage::onAssignPress(uint8_t index) noexcept
{
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
        if (!assignOk()) {
            return;
        }
        ui().mFile.refreshSaveBtn();
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
    refreshMenuButtons();
    refreshGroups(false);
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
    if (_recallSlot < GroupButtons::kCount) {
        onSelectNack();
        return;
    }
    if (e.tag == kTagBlockMsg) {
        refreshGroups(false);
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

bool WorkPage::assignOk() const noexcept
{
    return console.mode() == MConsole::Mode::Work && !_blockOn;
}

void WorkPage::showBlockMsg(const char* text) noexcept
{
    /* text — UTF-8 литерал из прошивки (не имя с SD). */
    ui().showUtf8Msg(uiMsg::kTitleBlock, nex::ovl::MsgBox::Preset::OK, kTagBlockMsg,
        nex::ovl::MsgBox::Action::Ok, text);
}

void WorkPage::showSharedWinchesMsg() noexcept
{
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
}

void WorkPage::refreshMenuButtons() noexcept
{
    using State = ConsoleBtn::State;
    const MConsole::Mode mode = console.mode();

    const State fileState = assignOk() ? State::Active : State::Disabled;
    bFile.setState(fileState);

    State blockState = State::Active;
    if (mode == MConsole::Mode::Show) {
        blockState = State::Disabled;
    } else if (_blockOn) {
        blockState = State::Selected;
    }
    bBlock.setState(blockState);

    const State showState = (mode == MConsole::Mode::Show) ? State::Selected : State::Active;
    bShow.setState(showState);
}

void WorkPage::refreshGroupBtn(bool textModified) noexcept
{
    static_assert(GroupButtons::kCount == GroupAssignButtons::kCount);
    const uint8_t active_id = console.shownGroup();
    const bool hasSelection = console.hasSelection();
    const bool enabled = assignOk();

    for (uint8_t i = 0; i < GroupButtons::kCount; ++i) {
        const uint8_t group_id = groupIdForSlot(i);
        if (group_id == MConsole::kNoActiveGroup) {
            groupBtn[i].sync(nullptr, false, textModified);
            groupAssignBtn[i].sync(nullptr, false, enabled, hasSelection);
            continue;
        }
        const smcp::CGroup grp = console.show.group[group_id];
        const bool selected = (group_id == active_id);
        groupBtn[i].sync(&grp, selected, textModified);
        groupAssignBtn[i].sync(&grp, selected, enabled, hasSelection);
    }
}

void WorkPage::refreshCells() noexcept
{
    static_assert(CellButtons::kCount == MConsole::kMechCount);
    for (uint8_t i = 0; i < CellButtons::kCount; ++i) {
        cells[i].sync(console);
    }
}

void WorkPage::refreshGroups(bool texts) noexcept
{
    if (texts) {
        refreshGroupPage();
    }
    refreshGroupBtn(texts);
}

void WorkPage::paintGroupSlot(uint8_t index, bool selected) noexcept
{
    if (index >= GroupButtons::kCount) {
        return;
    }
    const uint8_t group_id = groupIdForSlot(index);
    if (group_id == MConsole::kNoActiveGroup) {
        return;
    }
    const smcp::CGroup grp = console.show.group[group_id];
    if (grp.isEmpty()) {
        return;
    }
    groupBtn[index].sync(&grp, selected, false);
}

void WorkPage::onMechTelemetry(uint8_t mech_id) noexcept
{
    if (mech_id < CellButtons::kCount) {
        cells[mech_id].sync(console);
    }
}

void WorkPage::onGroupAck() noexcept
{
    _recallSlot = 0xFFu;
    _recallPrevSlot = 0xFFu;
}

void WorkPage::onSelectNack() noexcept
{
    if (_recallSlot >= GroupButtons::kCount) {
        return;
    }
    paintGroupSlot(_recallSlot, false);
    if (_recallPrevSlot < GroupButtons::kCount) {
        paintGroupSlot(_recallPrevSlot, true);
    }
    _recallSlot = 0xFFu;
    _recallPrevSlot = 0xFFu;
}

} // namespace server
