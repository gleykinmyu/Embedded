#include "browserPage.hpp"

#include "application.hpp"

#include <cstdio>
#include <cstring>

namespace server {

namespace {

void formatFatStamp(char* out, std::size_t outLen, uint16_t date, uint16_t time) noexcept
{
    if (out == nullptr || outLen == 0u) {
        return;
    }
    if (date == 0u && time == 0u) {
        out[0] = '\0';
        return;
    }

    const unsigned day = date & 0x1Fu;
    const unsigned month = (date >> 5) & 0x0Fu;
    const unsigned year = 1980u + ((date >> 9) & 0x7Fu);
    const unsigned hour = (time >> 11) & 0x1Fu;
    const unsigned min = (time >> 5) & 0x3Fu;

    std::snprintf(out, outLen, "%02u.%02u.%02u %02u:%02u",
        day, month, year % 100u, hour, min);
}

} // namespace

BrowserPage::BrowserPage(nex::IAppUI& app) noexcept
    : Page<37>(app, HMI_COMP_OBJNAME(browser), PG::kPageId)
{}

void BrowserPage::enterSaveAs() noexcept
{
    _forceSaveAs = true;

    /* Квалифицированный путь: страница browser ещё не активна. */
    static constexpr nex::Literal kBrowserModeVal{"browser.mode.val"};
    ui().setGlobalVar(kBrowserModeVal, static_cast<int32_t>(Mode::SaveAs));

    ui().switchPage(*this);
}

Application& BrowserPage::ui() const noexcept
{
    return static_cast<Application&>(app);
}

BrowserPage::Mode BrowserPage::currentMode() const noexcept
{
    const int32_t v = static_cast<int32_t>(mode.val);
    if (v == static_cast<int32_t>(Mode::SaveAs)) {
        return Mode::SaveAs;
    }
    if (v == static_cast<int32_t>(Mode::Delete)) {
        return Mode::Delete;
    }
    return Mode::Open;
}

std::size_t BrowserPage::visibleRows() const noexcept
{
    return (currentMode() == Mode::SaveAs) ? 7u : MBrowser::kPageSize;
}

void BrowserPage::onLoad()
{
    clearFileRowSelection();

    if (_forceSaveAs) {
        _forceSaveAs = false;
        /* После page-события: короткий `mode.val` уже на активной странице. */
        mode.val = static_cast<int32_t>(Mode::SaveAs);
        _pending = Pending::None;
        if (!mBrowser.refresh()) {
            ui().showBrowserStatus();
            return;
        }
        redrawRows();
        return;
    }

    _pending = Pending::ReadMode;
    mode.val.get();
}

void BrowserPage::onTouch(const nex::msg::evTouch& e)
{
    Page::onTouch(e);

    if (e.state != nex::TouchState::Release) {
        return;
    }

    using PB = nex::hmi::Page_browser;
    const uint8_t comp = e.route.comp;

    if (const std::size_t row = FileRows::indexOf(comp); row < FileRows::kCount) {
        onFileRow(row);
        return;
    }

    if (comp == PB::bFNext || comp == PB::bFPrev) {
        changePage(comp == PB::bFNext);
        return;
    }

    if (comp == PB::bAction) {
        onAction();
    }
}

void BrowserPage::onResponse(const nex::msg::getNumeric& response, nex::Route route, uint8_t tag)
{
    Page::onResponse(response, route, tag);

    if (_pending != Pending::ReadMode) {
        return;
    }
    if (route.comp != mode.id()) {
        return;
    }

    _pending = Pending::None;
    if (!mBrowser.refresh()) {
        ui().showBrowserStatus();
        return;
    }
    redrawRows();
}

void BrowserPage::onResponse(const nex::msg::getString& response, nex::Route route, uint8_t tag)
{
    Page::onResponse(response, route, tag);

    if (_pending != Pending::SaveAsName) {
        return;
    }
    if (route.comp != fNameStr.id()) {
        return;
    }

    _pending = Pending::None;
    finishSaveAs();
}

void BrowserPage::clearFileRowSelection() noexcept
{
    for (std::size_t i = 0; i < FileRows::kCount; ++i) {
        if (fileRows[i].getState() == BrowserBtn::State::Selected) {
            fileRows[i].setState(BrowserBtn::State::Active);
        }
    }
    mBrowser.clearSelection();
    BrowserBtn::active_id = 0xFFu;
}

void BrowserPage::updateStatusTexts() noexcept
{
    char buf[12]{};
    std::snprintf(buf, sizeof(buf), "%u/%u",
        static_cast<unsigned>(mBrowser.page() + 1u),
        static_cast<unsigned>(mBrowser.pageCount()));
    tfPage.txt.set(buf);

    std::snprintf(buf, sizeof(buf), "%u",
        static_cast<unsigned>(mBrowser.fileCount()));
    tfNum.txt.set(buf);
}

void BrowserPage::redrawRows() noexcept
{
    mBrowser.setPageRows(visibleRows());

    char stamp[20]{};
    const std::size_t rows = visibleRows();
    const std::size_t selected = mBrowser.selectedIndex();

    if (selected == MBrowser::npos) {
        clearFileRowSelection();
    }

    for (std::size_t i = 0; i < FileRows::kCount; ++i) {
        using BtnState = BrowserBtn::State;

        if (i >= rows) {
            fileRows[i].hide();
            continue;
        }

        const MBrowser::Entry* entry = mBrowser.entryAt(i);
        if (entry == nullptr) {
            fileRows[i].setText("");
            fileDates[i].txt.set("");
            fileRows[i].setState(BtnState::Disabled);
            continue;
        }

        fileRows[i].setText("  ");
        fileRows[i].appendText(entry->name);
        fileRows[i].appendText("\r ");
        const std::size_t base = mBrowser.page() * rows;
        const bool rowSelected = (selected != MBrowser::npos) && (selected == base + i);
        fileRows[i].setState(rowSelected ? BtnState::Selected : BtnState::Active);
        if (rowSelected) {
            BrowserBtn::active_id = fileRows[i].id();
        }
        formatFatStamp(stamp, sizeof(stamp), entry->date, entry->time);
        fileDates[i].txt.set(stamp);
    }

    updateStatusTexts();
}

void BrowserPage::onFileRow(std::size_t row) noexcept
{
    if (row >= visibleRows()) {
        return;
    }
    if (!mBrowser.selectRow(row)) {
        return;
    }

    if (currentMode() == Mode::SaveAs) {
        const char* name = mBrowser.selectedName();
        if (name[0] != '\0') {
            fNameStr.txt.set(name);
        }
    }
}

void BrowserPage::changePage(bool next) noexcept
{
    const bool moved = next ? mBrowser.pageNext() : mBrowser.pagePrev();
    if (!moved) {
        return;
    }
    clearFileRowSelection();
    redrawRows();
}

void BrowserPage::onAction() noexcept
{
    switch (currentMode()) {
    case Mode::Open:
        doOpen();
        break;
    case Mode::SaveAs:
        beginSaveAs();
        break;
    case Mode::Delete:
        doDelete();
        break;
    }
}

void BrowserPage::doOpen() noexcept
{
    char path[48]{};
    if (!mBrowser.prepareOpenSelected(path, sizeof(path))) {
        if (mBrowser.getStatus() == MBrowser::Status::NotFound) {
            redrawRows();
        }
        ui().showBrowserStatus();
        return;
    }

    if (console.isEdited()) {
        std::strncpy(_pendingPath, path, sizeof(_pendingPath) - 1u);
        _pendingPath[sizeof(_pendingPath) - 1u] = '\0';
        _msg = Msg::ConfirmDiscardOpen;
        ui().showFileYesNo(kTagDiscardOpen, "Show not saved.\nOpen anyway?");
        return;
    }

    commitOpen(path);
}

void BrowserPage::commitOpen(const char* path) noexcept
{
    if (!console.loadShow(mBrowser, path)) {
        ui().showConsoleStatus();
        return;
    }
    _msg = Msg::None;
    _pendingPath[0] = '\0';
    ui().switchPage(ui().work);
}

void BrowserPage::beginSaveAs() noexcept
{
    _pending = Pending::SaveAsName;
    fNameStr.txt.get();
}

void BrowserPage::finishSaveAs() noexcept
{
    const char* name = fNameStr.txt;
    if (!console.canMutateShowName(name)) {
        ui().showConsoleStatus();
        return;
    }

    char path[48]{};
    if (!mBrowser.prepareSaveAs(name, path, sizeof(path))) {
        if (mBrowser.getStatus() == MBrowser::Status::FileExists) {
            std::strncpy(_pendingPath, path, sizeof(_pendingPath) - 1u);
            _pendingPath[sizeof(_pendingPath) - 1u] = '\0';
            _msg = Msg::OverwriteSave;
            ui().showFileYesNo(kTagOverwriteSave, "Overwrite file?");
            return;
        }
        ui().showBrowserStatus();
        return;
    }

    commitSaveAs(path);
}

void BrowserPage::commitSaveAs(const char* path) noexcept
{
    if (!console.saveShow(mBrowser, path)) {
        ui().showConsoleStatus();
        return;
    }

    _msg = Msg::None;
    _pendingPath[0] = '\0';
    if (!mBrowser.refresh()) {
        ui().showBrowserStatus();
        return;
    }
    redrawRows();
    ui().switchPage(ui().work);
}

void BrowserPage::doDelete() noexcept
{
    if (mBrowser.selectedName()[0] == '\0') {
        ui().showFileMsg(0u, MBrowser::statusText(MBrowser::Status::NoSelection));
        return;
    }
    if (!console.canMutateShowName(mBrowser.selectedName())) {
        ui().showConsoleStatus();
        return;
    }

    _msg = Msg::ConfirmDelete;
    ui().showFileYesNo(kTagConfirmDelete, "Delete file?");
}

void BrowserPage::commitDelete() noexcept
{
    if (!mBrowser.removeSelected(console.showName())) {
        ui().showBrowserStatus();
        return;
    }

    _msg = Msg::None;
    ui().showFileMsg(0u, "File deleted.");
}

void BrowserPage::onMsgBox(const nex::msg::evMsgBox& e)
{
    if (e.action != nex::msg::evMsgBox::Action::Yes) {
        _msg = Msg::None;
        _pendingPath[0] = '\0';
        clearFileRowSelection();
        redrawRows();
        return;
    }

    switch (_msg) {
    case Msg::OverwriteSave:
        commitSaveAs(_pendingPath);
        break;
    case Msg::ConfirmDelete:
        commitDelete();
        break;
    case Msg::ConfirmDiscardOpen:
        commitOpen(_pendingPath);
        break;
    case Msg::None:
    default:
        break;
    }
}

} // namespace server
