#pragma once

#include <cstdint>

#include "buttons.hpp"
#include "nexHmiConfig.hpp"
#include "nex.hpp"

namespace server {

class Application;

struct MGroupPage : nex::Page<7> {
    HMI_PAGE_CFG(mGroup);

    HMI_COMP(ConsoleBtn, bClr);
    HMI_COMP(ConsoleBtn, bRen);
    HMI_COMP(ConsoleBtn, bRec);
    HMI_COMP(nex::comp::NumericVar, gbid);

    explicit MGroupPage(nex::IAppUI& app) noexcept;

    void onTouch(const nex::msg::evTouch& e) override;
    void onResponse(const nex::msg::getNumeric& response, nex::Route route, uint8_t tag) override;
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

    void requestGroupId(Action action) noexcept;
    void handleAction(uint8_t group_id) noexcept;
    void doRecord(uint8_t group_id) noexcept;
    void doClear(uint8_t group_id) noexcept;
    void doRename(uint8_t group_id) noexcept;
    void finishToWork() noexcept;

    Action _action = Action::None;
    uint8_t _groupId = 0xFFu;
};

} // namespace server
