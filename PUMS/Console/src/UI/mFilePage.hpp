#pragma once

#include <cstdint>

#include "buttons.hpp"
#include "nexHmiConfig.hpp"
#include "nex.hpp"

namespace server {

class Application;

struct MFilePage : nex::Page<9> {
    HMI_PAGE_CFG(mFile);

    HMI_COMP(ConsoleBtn, bSave);
    HMI_COMP(ConsoleBtn, bNew);

    explicit MFilePage(nex::IAppUI& app) noexcept;

    void onTouch(const nex::msg::evTouch& e) override;
    void onMsgBox(const nex::msg::evMsgBox& e) override;

private:
    enum Msg : uint8_t {
        None = 0,
        GoSaveAs,
        ConfirmNew,
        AfterSaveOk,
    };

    [[nodiscard]] Application& ui() const noexcept;

    void doSave() noexcept;
};

} // namespace server
