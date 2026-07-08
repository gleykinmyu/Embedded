#include "console.hpp"

#include <cstdio>

namespace server {

namespace {

constexpr uint8_t kScenePageCount = 4u;

} // namespace

Application& WorkPage::ui() const noexcept
{
    return static_cast<Application&>(app);
}

void WorkPage::onLoad()
{
    for (uint8_t i = 0; i < CellButtons::kCount; ++i) {
        cells[i].setState(ConsoleButton::State::Active);
    }
    refreshScenePageLabel();
}

void WorkPage::onTouch(const nex::msg::evTouch& e)
{
    if (e.state != nex::TouchState::Press) {
        return;
    }

    using Id = nex::hmi::Page_work::Id;
    const uint8_t comp = e.route.comp;

    switch (static_cast<Id>(comp)) {
    case Id::bSNext:
        onSceneNext();
        return;
    case Id::bSPrev:
        onScenePrev();
        return;
    case Id::bFile:
        onFilePress();
        return;
    case Id::bBlock:
        onBlockPress();
        return;
    case Id::bShow:
        onShowPress();
        return;
    default:
        break;
    }

    const uint8_t groupFirst = static_cast<uint8_t>(Id::bS0);
    const uint8_t groupLast = static_cast<uint8_t>(Id::bS7);
    if (comp >= groupFirst && comp <= groupLast) {
        onGroupPress(static_cast<uint8_t>(comp - groupFirst));
        return;
    }

    const uint8_t assignFirst = static_cast<uint8_t>(Id::bSa0);
    const uint8_t assignLast = static_cast<uint8_t>(Id::bSa7);
    if (comp >= assignFirst && comp <= assignLast) {
        onGroupAssignPress(static_cast<uint8_t>(comp - assignFirst));
        return;
    }

    const uint8_t cellFirst = static_cast<uint8_t>(Id::b0);
    const uint8_t cellLast = static_cast<uint8_t>(Id::b24);
    if (comp >= cellFirst && comp <= cellLast) {
        onCellPress(static_cast<uint8_t>(comp - cellFirst));
    }
}

void WorkPage::onCellPress(uint8_t index)
{
    if (index >= CellButtons::kCount) {
        return;
    }

    WinchButton& cell = cells[index];
    using State = ConsoleButton::State;

    if (!blockMode_) {
        switch (cell.getState()) {
        case State::Active:
            cell.setState(State::Selected);
            break;
        case State::Selected:
            cell.setState(State::Active);
            break;
        default:
            break;
        }
        return;
    }

    switch (cell.getState()) {
    case State::Active:
    case State::Selected:
        cell.setState(State::Blocked);
        break;
    case State::Blocked:
        cell.setState(State::Active);
        break;
    default:
        break;
    }
}

void WorkPage::onGroupPress(uint8_t index)
{
    if (index >= GroupButtons::kCount) {
        return;
    }

    selectedScene_ = index;
    clearCellSelection();
}

void WorkPage::onGroupAssignPress(uint8_t index)
{
    (void)index;
    ui().switchPage(HMI_COMP_OBJNAME(mGroup));
}

void WorkPage::onSceneNext()
{
    scenePage_ = static_cast<uint8_t>((scenePage_ + 1u) % kScenePageCount);
    refreshScenePageLabel();
}

void WorkPage::onScenePrev()
{
    scenePage_ = static_cast<uint8_t>((scenePage_ + kScenePageCount - 1u) % kScenePageCount);
    refreshScenePageLabel();
}



void WorkPage::onBlockPress()
{
    blockMode_ = !blockMode_;
    if (!blockMode_) {
        clearCellSelection();
    }
}

void WorkPage::onShowPress()
{
 
}

void WorkPage::refreshScenePageLabel() noexcept
{
    char label[8];
    std::snprintf(label, sizeof label, "%u", static_cast<unsigned>(scenePage_ + 1u));
    tgPage.setText(label);
}

void WorkPage::clearCellSelection() noexcept
{
    for (uint8_t i = 0; i < CellButtons::kCount; ++i) {
        WinchButton& cell = cells[i];
        const auto state = cell.getState();
        if (state == ConsoleButton::State::Selected) {
            cell.setState(ConsoleButton::State::Active);
        } else if (state == ConsoleButton::State::Blocked && !blockMode_) {
            cell.setState(ConsoleButton::State::Active);
        }
    }
}

} // namespace server
