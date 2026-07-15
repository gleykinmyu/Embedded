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
    enum class Msg : uint8_t {
        None = 0,
        GoSaveAs,
        ConfirmNew,
        AfterSaveOk,
    };

    static constexpr uint8_t kTagGoSaveAs = 1u;
    static constexpr uint8_t kTagConfirmNew = 2u;
    static constexpr uint8_t kTagAfterSave = 3u;

    [[nodiscard]] Application& ui() const noexcept;

    void doSave() noexcept;
    void beginNew() noexcept;
    void commitNew() noexcept;
    void goSaveAs() noexcept;
    void finishToWork() noexcept;

    void showFileMsg(uint8_t tag, const char* text) noexcept;
    void showFileYesNo(uint8_t tag, const char* text) noexcept;
    void showFsError() noexcept;

    Msg _msg = Msg::None;
};

} // namespace server
