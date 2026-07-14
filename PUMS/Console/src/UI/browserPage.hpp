#pragma once

#include <cstdint>

#include "buttons.hpp"
#include "nexHmiConfig.hpp"
#include "nex.hpp"

namespace server {

class Application;

struct BrowserPage : nex::Page<37> {
    HMI_PAGE_CFG(browser);

    HMI_INPLACE_PAGE_RANGE_CFG(FileRows, browser, nex::comp::Text<>, bF0, bF7)
    HMI_INPLACE_PAGE_RANGE_CFG(FileDateRows, browser, nex::comp::Text<>, bF0d, bF7d)

    HMI_COMP(ConsoleBtn, bAction);
    HMI_COMP(ConsoleBtn, bFNext);
    HMI_COMP(ConsoleBtn, bFPrev);

    HMI_COMP(ConsoleBtn, b0);

    HMI_COMP(nex::comp::NumericVar, mode);
    HMI_COMP(nex::comp::StringVar<32>, fNameStr);

    FileRows fileRows{*this};
    FileDateRows fileDates{*this};

    explicit BrowserPage(nex::IAppUI& app) noexcept;

    void onLoad() override;
    void onTouch(const nex::msg::evTouch& e) override;
    void onResponse(const nex::msg::getNumeric& response, nex::Route route, uint8_t tag) override;
    void onResponse(const nex::msg::getString& response, nex::Route route, uint8_t tag) override;

private:
    enum class Mode : int32_t {
        Open = 0,
        SaveAs = 1,
        Delete = 2,
    };

    enum class Pending : uint8_t {
        None = 0,
        ReadMode,
        SaveAsName,
    };

    [[nodiscard]] Application& ui() const noexcept;

    [[nodiscard]] Mode currentMode() const noexcept;
    [[nodiscard]] std::size_t visibleRows() const noexcept;

    void applyModeLayout() noexcept;
    void refreshList() noexcept;
    void redrawRows() noexcept;

    void onFileRow(std::size_t row) noexcept;
    void onPageNext() noexcept;
    void onPagePrev() noexcept;
    void onAction() noexcept;

    void doOpen() noexcept;
    void beginSaveAs() noexcept;
    void finishSaveAs() noexcept;
    void doDelete() noexcept;

    Pending _pending = Pending::None;
};

} // namespace server
