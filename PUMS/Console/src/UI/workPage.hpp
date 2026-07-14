#pragma once

#include <cstdint>

#include "buttons.hpp"
#include "nexHmiConfig.hpp"
#include "nex.hpp"

namespace server {

using namespace nex::comp;

struct WorkPage : nex::Page<54> {
    HMI_PAGE_CFG(work);

    HMI_INPLACE_PAGE_RANGE_CFG(GroupButtons, work, GroupBtn, bS0, bS7)
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

    //HMI_COMP(StringVar<32>, gName);

    StringVar<32> gName {*this, "work.gName", 54};

    explicit WorkPage(nex::IAppUI& app) noexcept;

    void onTouch(const nex::msg::evTouch& e) override;
    void onLoad() override;
    void onResponse(const nex::msg::getString& response, nex::Route route, uint8_t tag) override;

    [[nodiscard]] uint8_t groupIdForSlot(uint8_t index) const noexcept;

    void beginRename(uint8_t group_id) noexcept;

private:
    void onCellPress(uint8_t index, nex::TouchState state);
    void onGroupPress(uint8_t comp, nex::TouchState state);
    void onMenuPress(uint8_t comp);

    void refreshGroupBtn(bool textModified) noexcept;
    void refreshCell(uint8_t index) noexcept;
    void refreshCells() noexcept;

    void finishRename() noexcept;

    uint8_t _scenePage = 0u;
    uint8_t _renameGroupId = 0xFFu;
};

} // namespace server
