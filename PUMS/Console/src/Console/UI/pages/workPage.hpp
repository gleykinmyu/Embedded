#pragma once

#include <cstdint>

#include "UI/buttons.hpp"
#include "UI/nexHmiConfig.hpp"
#include "model/mconsole.hpp"
#include "nex.hpp"

namespace server {

class Application;

using namespace nex::comp;

struct WorkPage : nex::Page<54> {
    HMI_PAGE_CFG(work);

    using GroupButtons = InplaceArray<GroupBtn,
        PG::bS0, PG::bS1, PG::bS2, PG::bS3,
        PG::bS4, PG::bS5, PG::bS6, PG::bS7>;

    using GroupAssignButtons = InplaceArray<AssignBtn,
        PG::bSa0, PG::bSa1, PG::bSa2, PG::bSa3,
        PG::bSa4, PG::bSa5, PG::bSa6, PG::bSa7>;

    using CellButtons = InplaceArray<WinchButton,
        PG::b0, PG::b1, PG::b2, PG::b3,
        PG::b4, PG::b5, PG::b6, PG::b7,
        PG::b8, PG::b9, PG::b10, PG::b11,
        PG::b12, PG::b13, PG::b14, PG::b15,
        PG::b16, PG::b17, PG::b18, PG::b19,
        PG::b20, PG::b21, PG::b22, PG::b23>;

    HMI_COMP(ConsoleBtn, bFile);
    HMI_COMP(ConsoleBtn, bBlock);
    HMI_COMP(ConsoleBtn, bShow);

    GroupButtons groupBtn{*this};
    GroupAssignButtons groupAssignBtn{*this};
    CellButtons cells{*this};

    HMI_COMP_GLOBAL(PageLabelText, tgPage);
    HMI_COMP(ConsoleBtn, bSNext);
    HMI_COMP(ConsoleBtn, bSPrev);

    HMI_COMP_GLOBAL(StringVar<32>, gName);

    explicit WorkPage(nex::IAppUI& app) noexcept;

    void onTouch(const nex::msg::evTouch& e) override;
    void onLoad() override;
    void onResponse(const nex::msg::getString& response, nex::Route route, uint8_t tag) override;
    void onMsgBox(const nex::msg::evMsgBox& e) override;
    void onAfterMsgBox(const nex::msg::evMsgBox& e) override;

    [[nodiscard]] uint8_t groupIdForSlot(uint8_t index) const noexcept;

    void beginRename(uint8_t group_id) noexcept;

    /** Global: tgPage + группы/assign. Можно до показа work. */
    void refreshGroups(bool texts) noexcept;
    void refreshCells() noexcept;

    /** Telemetry → клетка. Recall группу не перекрашивает. */
    void onMechTelemetry(uint8_t mech_id) noexcept;
    /** Select Ack после recall: билет закрыт, подсветка уже на кнопке. */
    void onGroupAck() noexcept;
    /** Select Nack / отказ recall: откат кнопки после MsgBox (`onAfterMsgBox`). */
    void onSelectNack() noexcept;

private:
    static constexpr uint8_t kTagExitShow = 1u;
    static constexpr uint8_t kTagBlockMsg = 2u;

    [[nodiscard]] Application& ui() const noexcept;
    /** Work и не Block: кнопки assign активны. */
    [[nodiscard]] bool assignOk() const noexcept;

    void onCellPress(uint8_t index, nex::TouchState state);
    void onGroupPress(uint8_t comp, nex::TouchState state);
    void onAssignPress(uint8_t index) noexcept;
    void onMenuPress(uint8_t comp);

    void refreshMenuButtons() noexcept;
    void refreshGroupPage() noexcept;
    void refreshGroupBtn(bool textModified) noexcept;
    void paintGroupSlot(uint8_t index, bool selected) noexcept;

    void showBlockMsg(const char* text) noexcept;
    void showSharedWinchesMsg() noexcept;
    void showExitShowConfirm() noexcept;
    void applyModeChange() noexcept;

    void finishRename() noexcept;

    uint8_t _scenePage = 0u;
    uint8_t _renameGroupId = 0xFFu;
    uint8_t _recallSlot = 0xFFu;
    uint8_t _recallPrevSlot = 0xFFu;
    bool _blockOn = false;
};

} // namespace server
