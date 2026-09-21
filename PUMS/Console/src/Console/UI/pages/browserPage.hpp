#pragma once

#include <cstddef>
#include <cstdint>

#include "UI/buttons.hpp"
#include "UI/nexHmiConfig.hpp"
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

    HMI_COMP(PageLabelText, tfPage);
    HMI_COMP(PageLabelText, tfNum);

    HMI_COMP_GLOBAL(nex::comp::NumericVar, mode);
    HMI_COMP(nex::comp::StringVar<32>, fNameStr);

    FileRows fileRows{*this};
    FileDateRows fileDates{*this};

    explicit BrowserPage(nex::IAppUI& app) noexcept;

    /** Открыть браузер в Save As (`_forceSaveAs` + `onLoad`). */
    void enterSaveAs() noexcept;

    void onLoad() override;
    void onTouch(const nex::msg::evTouch& e) override;
    void onResponse(const nex::msg::getNumeric& response, nex::Route route, uint8_t tag) override;
    void onResponse(const nex::msg::getString& response, nex::Route route, uint8_t tag) override;
    void onMsgBox(const nex::msg::evMsgBox& e) override;
    void onAfterMsgBox(const nex::msg::evMsgBox& e) override;

    /** CD сменился, пока открыт браузер: refresh + все ячейки. Без MsgBox. */
    void reloadOnCardChange() noexcept;

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
    void commitOpen() noexcept;
    void beginSaveAs() noexcept;
    void finishSaveAs() noexcept;
    void commitSaveAs() noexcept;
    void afterSaveAsOk() noexcept;
    void doDelete() noexcept;
    void commitDelete() noexcept;

    static constexpr std::size_t kPageSize = 8u;
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    Pending _pending = Pending::None;
    Msg _msg = Msg::None;
    bool _forceSaveAs = false;
    std::size_t _page = 0u;
    std::size_t _selected = npos;
    char _saveAsName[32]{};
};

} // namespace server
