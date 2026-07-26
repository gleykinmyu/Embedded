#pragma once

#include <cstdint>

#include "UI/buttons.hpp"
#include "UI/nexHmiConfig.hpp"
#include "nex.hpp"

namespace server {

class Application;

struct MFilePage : nex::Page<10> {
    HMI_PAGE_CFG(mFile);

    HMI_COMP(ConsoleBtn, bSave);
    HMI_COMP(ConsoleBtn, bNew);
    HMI_COMP(ConsoleBtn, bSettings);

    explicit MFilePage(nex::IAppUI& app) noexcept;

    void onLoad() override;
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

    void refreshSaveBtn() noexcept;
    void doSave() noexcept;
    void beginNew() noexcept;
    void commitNew() noexcept;
};

} // namespace server
