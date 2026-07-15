#pragma once

#include <cstdint>

#include "buttons.hpp"
#include "nexHmiConfig.hpp"
#include "nex.hpp"

namespace server {

class Application;

struct BrowserPage : nex::Page<37> {
    HMI_PAGE_CFG(browser);

    using FileRows = InplaceArray<BrowserBtn,
        PG::bF0, PG::bF1, PG::bF2, PG::bF3,
        PG::bF4, PG::bF5, PG::bF6, PG::bF7>;

    using FileDateRows = InplaceArray<FileDateText,
        PG::bF0d, PG::bF1d, PG::bF2d, PG::bF3d,
        PG::bF4d, PG::bF5d, PG::bF6d, PG::bF7d>;

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
    void onMsgBox(const nex::msg::evMsgBox& e) override;

    [[nodiscard]] bool takeDeleteDonePending() noexcept;
    void showDeleteDoneMsg() noexcept;

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
        DeleteDoneMsg,
    };

    enum class Msg : uint8_t {
        None = 0,
        OverwriteSave,
        ConfirmDelete,
    };

    static constexpr uint8_t kTagOverwriteSave = 1u;
    static constexpr uint8_t kTagConfirmDelete = 2u;

    [[nodiscard]] Application& ui() const noexcept;

    [[nodiscard]] Mode currentMode() const noexcept;
    [[nodiscard]] std::size_t visibleRows() const noexcept;

    void redrawRows() noexcept;
    void clearFileRowSelection() noexcept;

    void onFileRow(std::size_t row) noexcept;
    void changePage(bool next) noexcept;
    void onAction() noexcept;

    void doOpen() noexcept;
    void beginSaveAs() noexcept;
    void finishSaveAs() noexcept;
    void commitSaveAs(const char* path) noexcept;
    void doDelete() noexcept;
    void commitDelete() noexcept;

    void showFileMsg(const char* text) noexcept;
    void showFileYesNo(uint8_t tag, const char* text) noexcept;
    void showFsError() noexcept;

    Pending _pending = Pending::None;
    Msg _msg = Msg::None;
    char _pendingPath[48]{};
};

} // namespace server
