#pragma once

#include <cstdint>

#include "UI/buttons.hpp"
#include "UI/nexHmiConfig.hpp"
#include "nex.hpp"

namespace server {

class Application;

using namespace nex::comp;

struct SettingsPage : nex::Page<23> {
    HMI_PAGE_CFG(settings);

    HMI_COMP(ConsoleBtn, bAction);
    HMI_COMP(ConsoleBtn, bCancel);
    HMI_COMP(ToggleSwitch, swGr0);

    HMI_COMP(Number<>, nThour);
    HMI_COMP(Number<>, nTmin);
    HMI_COMP(Number<>, nTsec);
    HMI_COMP(Number<>, nDday);
    HMI_COMP(Number<>, nDmon);
    HMI_COMP(Number<>, nDyear);

    explicit SettingsPage(nex::IAppUI& app) noexcept;

    /** Вызвать до onLoad: возврат с keybdB после правки даты/времени. */
    void noteReturnFromKeybd() noexcept;

    void onLoad() override;
    void onTouch(const nex::msg::evTouch& e) override;
    void onResponse(const nex::msg::getNumeric& response, nex::Route route, uint8_t tag) override;
    void onMsgBox(const nex::msg::evMsgBox& e) override;

private:
    enum class Pending : uint8_t {
        None = 0,
        ValidateAfterKeybd,
        ApplyAll,
    };

    enum Msg : uint8_t {
        None = 0,
        AfterClampField,
        AfterClampApply,
    };

    [[nodiscard]] Application& ui() const noexcept;

    void loadRtcToUi() noexcept;
    void requestAllFields() noexcept;
    void beginValidateAfterKeybd() noexcept;
    void commitValidateAfterKeybd() noexcept;
    void beginApply() noexcept;
    void commitApply() noexcept;
    void finishToWork() noexcept;

    /** Clamp полей на панели; true если было вне диапазона. */
    [[nodiscard]] bool clampAllFields() noexcept;

    bool _returnFromKeybd = false;
    Pending _pending = Pending::None;
    uint8_t _pendingGets = 0u;
};

} // namespace server
