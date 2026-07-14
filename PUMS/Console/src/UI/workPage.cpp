#include "workPage.hpp"

#include "application.hpp"

namespace server {

namespace {

constexpr uint8_t kScenePageCount = 4u;

} // namespace

WorkPage::WorkPage(nex::IAppUI& app) noexcept
    : Page<54>(app, HMI_COMP_OBJNAME(work), PageCfg::kPageId)
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
    if (state != nex::TouchState::Press) {
        return;
    }
    if (index >= CellButtons::kCount) {
        return;
    }

    if (console.select(index)) {
        refreshCell(index);
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
    if (grp.isEmpty() || grp.isBlocked()) {
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

    if (group_id == console.getActiveGroup()) {
        console.clearActiveGroup();
    } else if (!console.recallGroup(group_id)) {
        return;
    }

    refreshGroupBtn(false);
    refreshCells();
}

void WorkPage::onMenuPress(uint8_t comp)
{
    using PW = nex::hmi::Page_work;

    switch (comp) {
    case PW::bFile:
        break;
    case PW::bBlock:
        break;
    case PW::bShow:
        break;
    default:
        break;
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

        const State assignNext = (next == State::Disabled) ? State::Active : next;
        if (groupAssignBtn[i].getState() != assignNext) {
            groupAssignBtn[i].setState(assignNext);
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
    if (console.mechGroupMask(index, smcp::Group::Flag::Blocked) != 0ULL) {
        next = State::Blocked;
    } else if (console.mech(index).status().all(smcp::IMech::Status::Selected | smcp::IMech::Status::Ready)) {
        next = State::Selected;
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

} // namespace server
