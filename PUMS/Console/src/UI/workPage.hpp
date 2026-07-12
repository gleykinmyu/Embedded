#pragma once

#include <cstdint>

#include "buttons.hpp"
#include "nexHmiConfig.hpp"
#include "nex.hpp"

namespace smcp {
struct Group;
}

namespace server {

using namespace nex::comp;

class Application;

struct WorkPage : nex::Page<54> {
    HMI_PAGE_CFG(work);

    HMI_INPLACE_PAGE_RANGE_CFG(GroupButtons, work, ConsoleBtn, bS0, bS7)
    HMI_INPLACE_PAGE_RANGE_CFG(GroupAssignButtons, work, ConsoleBtn, bSa0, bSa7)
    HMI_INPLACE_PAGE_RANGE_CFG(CellButtons, work, WinchButton, b0, b23)

    HMI_COMP(ConsoleBtn, bFile);
    HMI_COMP(ConsoleBtn, bBlock);
    HMI_COMP(ConsoleBtn, bShow);

    GroupButtons groupBtn{*this};
    GroupAssignButtons groupAssignBtn{*this};
    CellButtons cells{*this};

    HMI_COMP(Text<>, tgPage);
    HMI_COMP(ConsoleBtn, bSNext);
    HMI_COMP(ConsoleBtn, bSPrev);

    HMI_COMP(StringVar<32>, gName);

    explicit WorkPage(nex::IAppUI& app) noexcept;

    void onTouch(const nex::msg::evTouch& e) override;
    void onLoad() override;

private:
    [[nodiscard]] Application& ui() const noexcept;

    [[nodiscard]] uint8_t groupIdForSlot(uint8_t index) const noexcept;

    void onCellPress(uint8_t index);
    void onGroupPress(uint8_t index);
    void onGroupAssignPress(uint8_t index);
    void onSceneNext();
    void onScenePrev();
    void onFilePress();
    void onBlockPress();
    void onShowPress();

    void refreshScenePageLabel() noexcept;
    void refreshGroupButtons() noexcept;
    void refreshGroupName(uint8_t group_id) noexcept;
    void refreshCell(uint8_t index) noexcept;
    void refreshCells() noexcept;
    void refreshCellFromGroup(uint8_t index, const smcp::Group& grp) noexcept;

    bool _blockMode = false;
    uint8_t _scenePage = 0u;
    uint8_t _selectedScene = 0xFFu;
};

} // namespace server
