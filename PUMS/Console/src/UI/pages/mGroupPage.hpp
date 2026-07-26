#pragma once

#include <cstdint>

#include "UI/buttons.hpp"
#include "UI/nexHmiConfig.hpp"
#include "nex.hpp"

namespace server {

class Application;

struct MGroupPage : nex::Page<3> {
    HMI_PAGE_CFG(mGroup);

    HMI_COMP(ConsoleBtn, bClr);
    HMI_COMP(ConsoleBtn, bRen);
    HMI_COMP(ConsoleBtn, bRec);

    explicit MGroupPage(nex::IAppUI& app) noexcept;

    /** Слот Assign на work (0..7), задаётся до switchPage. */
    void setAssignSlot(uint8_t slot) noexcept;

    void onLoad() override;
    void onTouch(const nex::msg::evTouch& e) override;
    void onMsgBox(const nex::msg::evMsgBox& e) override;

private:
    enum class Action : uint8_t {
        None = 0,
        Record,
        Clear,
        Rename,
    };

    static constexpr uint8_t kTagOverwrite = 1u;
    static constexpr uint8_t kTagClear = 2u;

    [[nodiscard]] Application& ui() const noexcept;

    [[nodiscard]] uint8_t resolveGroupId() const noexcept;

    void refreshActionButtons() noexcept;
    void runAction(Action action) noexcept;
    void handleAction(uint8_t group_id) noexcept;
    void doRename(uint8_t group_id) noexcept;
    void finishToWork() noexcept;

    Action _action = Action::None;
    uint8_t _assignSlot = 0xFFu;
    uint8_t _groupId = 0xFFu;
};

} // namespace server
