#include "workPage.hpp"

#include "application.hpp"

namespace server {

namespace {

constexpr uint8_t kScenePageCount = 4u;

} // namespace

WorkPage::WorkPage(nex::IAppUI& app) noexcept
    : Page<54>(app, HMI_COMP_OBJNAME(work), PageCfg::kPageId)
{}

Application& WorkPage::ui() const noexcept
{
    return static_cast<Application&>(app);
}

uint8_t WorkPage::groupIdForSlot(uint8_t index) const noexcept
{
    return static_cast<uint8_t>(1u + static_cast<uint8_t>(_scenePage * GroupButtons::kCount) + index);
}

void WorkPage::onLoad()
{

}

void WorkPage::onTouch(const nex::msg::evTouch& e)
{
    if (e.state != nex::TouchState::Release) {
        return;
    }

    using PW = nex::hmi::Page_work;
    const uint8_t comp = e.route.comp;
    
    if (e.state == nex::TouchState::Release) {

    switch (comp) {
    case PW::bSNext:
        onSceneNext();
        return;
    case PW::bSPrev:
        onScenePrev();
        return;
    case PW::bFile:
        onFilePress();
        return;
    case PW::bBlock:
        onBlockPress();
        return;
    case PW::bShow:
        onShowPress();
        return;
    default:
        break;
    }

    const uint8_t groupFirst = PW::bS0;
    const uint8_t groupLast = PW::bS7;
    if (comp >= groupFirst && comp <= groupLast) {
        onGroupPress(static_cast<uint8_t>(comp - groupFirst));
        return;
    }

    const uint8_t assignFirst = PW::bSa0;
    const uint8_t assignLast = PW::bSa7;
    if (comp >= assignFirst && comp <= assignLast) {
        onGroupAssignPress(comp - assignFirst);
        return;
    }
}

    if (e.state == nex::TouchState::Release) {

    const uint8_t cellFirst = PW::b0;
    const uint8_t cellLast = PW::b23;
    if (comp >= cellFirst && comp <= cellLast) {
        onCellPress(comp - cellFirst);
    }
}
}

void WorkPage::onCellPress(uint8_t index)
{
    if (index >= CellButtons::kCount) {
        return;
    }

    if (_blockMode) {
        smcp::Group& blocked = console.group(MConsole::kBlockedGroupId);
        smcp::Selection selection = blocked.mech;

        if (selection.contains(index)) {
            selection.remove(index);
        } else {
            selection.add(index);
        }

        blocked.setSelection(selection);
        refreshCell(index);
        return;
    }

    if (console.select(index)) refreshCell(index);
}

void WorkPage::onGroupPress(uint8_t index)
{
    if (index >= GroupButtons::kCount) {
        return;
    }

    _selectedScene = index;
    const uint8_t group_id = groupIdForSlot(index);
    const smcp::Group& grp = console.group(group_id);

    if (grp.isEmpty()) {
        refreshCells();
        refreshGroupName(group_id);
        return;
    }

    if (_blockMode) {
        for (uint8_t i = 0; i < CellButtons::kCount; ++i) {
            refreshCellFromGroup(i, grp);
        }
        refreshGroupName(group_id);
        return;
    }

    if (console.recallGroup(group_id)) {
        refreshCells();
        refreshGroupName(group_id);
    }
}

void WorkPage::onGroupAssignPress(uint8_t index)
{
    (void)index;
    ui().switchPage(HMI_COMP_OBJNAME(mGroup));
}

void WorkPage::onSceneNext()
{
    _scenePage = static_cast<uint8_t>((_scenePage + 1u) % kScenePageCount);
    refreshScenePageLabel();
    refreshGroupButtons();
}

void WorkPage::onScenePrev()
{
    _scenePage = static_cast<uint8_t>((_scenePage + kScenePageCount - 1u) % kScenePageCount);
    refreshScenePageLabel();
    refreshGroupButtons();
}

void WorkPage::onFilePress()
{
}

void WorkPage::onBlockPress()
{
}

void WorkPage::onShowPress()
{
}

void WorkPage::refreshScenePageLabel() noexcept
{
    static constexpr const char* kPageLabels[] = {"1/4", "2/4", "3/4", "4/4"};

    const char* label = kPageLabels[0];
    if (_scenePage < kScenePageCount) {
        label = kPageLabels[_scenePage];
    }

    tgPage.setText(label);
}

void WorkPage::refreshGroupButtons() noexcept
{
    for (uint8_t i = 0; i < GroupButtons::kCount; ++i) {
        const smcp::Group& grp = console.group(groupIdForSlot(i));
        groupBtn[i].setText(grp.isEmpty() ? "-" : grp.name);
        groupBtn[i].setState(grp.isEmpty() ? ConsoleBtn::State::Disabled : ConsoleBtn::State::Active);
    }
}

void WorkPage::refreshGroupName(uint8_t group_id) noexcept
{
    const smcp::Group& grp = console.group(group_id);
    gName.txt.set(grp.isEmpty() ? "" : grp.name);
}

void WorkPage::refreshCell(uint8_t index) noexcept
{
    if (index >= CellButtons::kCount) {
        return;
    }

    WinchButton& cell = cells[index];
    using State = ConsoleBtn::State;

    if (console.mechGroupMask(index, smcp::Group::Flag::Blocked) != 0ULL) {
        cell.setState(State::Blocked);
        return;
    }

    if (console.mech(index).status().any(smcp::IMech::Status::Selected)) {
        cell.setState(State::Selected);
        return;
    }

    if (console.mech(index).status().any(smcp::IMech::Status::Ready)) {
        cell.setState(State::Active);
    } else {
        cell.setState(State::Disabled);
    }
}

void WorkPage::refreshCells() noexcept
{
    for (uint8_t i = 0; i < CellButtons::kCount; ++i) {
        refreshCell(i);
    }
}

void WorkPage::refreshCellFromGroup(uint8_t index, const smcp::Group& grp) noexcept
{
    if (index >= CellButtons::kCount) {
        return;
    }

    WinchButton& cell = cells[index];
    cell.setState(grp.mech.contains(index) ? ConsoleBtn::State::Blocked
                                           : ConsoleBtn::State::Active);
}

} // namespace server
