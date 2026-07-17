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

    HMI_COMP(ConsoleBtn, bCancel);

    HMI_COMP(nex::comp::Text<>, tfPage);
    HMI_COMP(nex::comp::Text<>, tfNum);

    HMI_COMP(nex::comp::NumericVar, mode);
    HMI_COMP(nex::comp::StringVar<32>, fNameStr);

    FileRows fileRows{*this};
    FileDateRows fileDates{*this};

    explicit BrowserPage(nex::IAppUI& app) noexcept;

    /** Открыть браузер в Save As: выставляет `browser.mode.val` и переключает страницу. */
    void enterSaveAs() noexcept;

    void onLoad() override;
    void onTouch(const nex::msg::evTouch& e) override;
    void onResponse(const nex::msg::getNumeric& response, nex::Route route, uint8_t tag) override;
    void onResponse(const nex::msg::getString& response, nex::Route route, uint8_t tag) override;
    void onMsgBox(const nex::msg::evMsgBox& e) override;

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

    enum class Msg : uint8_t {
        None = 0,
        OverwriteSave,
        ConfirmDelete,
        ConfirmDiscardOpen,
    };

    static constexpr uint8_t kTagOverwriteSave = 1u;
    static constexpr uint8_t kTagConfirmDelete = 2u;
    static constexpr uint8_t kTagDiscardOpen = 3u;

    [[nodiscard]] Application& ui() const noexcept;

    [[nodiscard]] Mode currentMode() const noexcept;
    [[nodiscard]] std::size_t visibleRows() const noexcept;

    void redrawRows() noexcept;
    void updateStatusTexts() noexcept;
    void clearFileRowSelection() noexcept;

    void onFileRow(std::size_t row) noexcept;
    void changePage(bool next) noexcept;
    void onAction() noexcept;

    void doOpen() noexcept;
    void commitOpen(const char* path) noexcept;
    void beginSaveAs() noexcept;
    void finishSaveAs() noexcept;
    void commitSaveAs(const char* path) noexcept;
    void doDelete() noexcept;
    void commitDelete() noexcept;

    Pending _pending = Pending::None;
    Msg _msg = Msg::None;
    char _pendingPath[48]{};
    bool _forceSaveAs = false;
};

} // namespace server
