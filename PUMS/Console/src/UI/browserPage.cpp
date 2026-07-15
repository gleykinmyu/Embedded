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

[[nodiscard]] bool isTemplateName(const char* name) noexcept
{
    static constexpr char kTemplate[] = {
        static_cast<char>(0xFB), static_cast<char>(0xE1), static_cast<char>(0xE2),
        static_cast<char>(0xEC), static_cast<char>(0xEF), static_cast<char>(0xEE),
        '\0',
    };
    return std::strncmp(name, kTemplate, sizeof(kTemplate) - 1u) == 0;
}

[[nodiscard]] bool isOpenShowPath(const char* path) noexcept
{
    const char* const show = console.showName();
    return show[0] != '\0' && std::strcmp(path, show) == 0;
}

} // namespace

void BrowserPage::showFileMsg(const char* text) noexcept
{
    ui().msgBox.setRoute(nex::Route{PG::kPageId, 0u});
    ui().msgBox.show("File", nex::ovl::MsgBox::Preset::OK, 0u,
        nex::ovl::MsgBox::Action::None, "%s", text);
}

void BrowserPage::showFileYesNo(uint8_t tag, const char* text) noexcept
{
    ui().msgBox.setRoute(nex::Route{PG::kPageId, 0u});
    ui().msgBox.show("File", nex::ovl::MsgBox::Preset::YesNo, tag,
        nex::ovl::MsgBox::Action::No, "%s", text);
}

void BrowserPage::showFsError() noexcept
{
    ui().msgBox.setRoute(nex::Route{PG::kPageId, 0u});
    ui().msgBox.show("File", nex::ovl::MsgBox::Preset::OK, 0u,
        nex::ovl::MsgBox::Action::None, "Storage error.");
}

bool BrowserPage::takeDeleteDonePending() noexcept
{
    if (_pending != Pending::DeleteDoneMsg) {
        return false;
    }
    _pending = Pending::None;
    return true;
}

void BrowserPage::showDeleteDoneMsg() noexcept
{
    showFileMsg("File deleted.");
}

BrowserPage::BrowserPage(nex::IAppUI& app) noexcept
    : Page<37>(app, HMI_COMP_OBJNAME(browser), PG::kPageId)
{}

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

    if (comp == PB::bFNext) {
        changePage(true);
        return;
    }
    if (comp == PB::bFPrev) {
        changePage(false);
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
        showFsError();
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
        NEX_DBG("stamp: %s\n", stamp);
        fileDates[i].txt.set(stamp);
    }
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
    if (!mBrowser.makeSelectedPath(path, sizeof(path))) {
        showFileMsg("Select a file.");
        return;
    }
    if (!mBrowser.volume().exists(path)) {
        showFileMsg("File not found.");
        (void)mBrowser.refresh();
        redrawRows();
        return;
    }
    if (!console.loadShow(path)) {
        showFsError();
        return;
    }
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
    if (name[0] == '\0') {
        showFileMsg("Enter file name.");
        return;
    }
    if (isTemplateName(name)) {
        showFileMsg("Cannot use template name.");
        return;
    }

    char path[48]{};
    if (!mBrowser.makePath(path, sizeof(path), name)) {
        showFileMsg("Invalid file name.");
        return;
    }
    if (mBrowser.volume().exists(path)) {
        std::strncpy(_pendingPath, path, sizeof(_pendingPath) - 1u);
        _pendingPath[sizeof(_pendingPath) - 1u] = '\0';
        _msg = Msg::OverwriteSave;
        showFileYesNo(kTagOverwriteSave, "Overwrite file?");
        return;
    }

    commitSaveAs(path);
}

void BrowserPage::commitSaveAs(const char* path) noexcept
{
    if (!console.saveShow(path)) {
        showFsError();
        return;
    }

    _msg = Msg::None;
    _pendingPath[0] = '\0';
    if (!mBrowser.refresh()) {
        showFsError();
        return;
    }
    redrawRows();
    ui().switchPage(ui().work);
}

void BrowserPage::doDelete() noexcept
{
    const char* name = mBrowser.selectedName();
    if (name[0] == '\0') {
        showFileMsg("Select a file.");
        return;
    }
    if (isTemplateName(name)) {
        showFileMsg("Cannot use template name.");
        return;
    }

    char path[48]{};
    if (!mBrowser.makeSelectedPath(path, sizeof(path))) {
        showFileMsg("Select a file.");
        return;
    }
    if (isOpenShowPath(path)) {
        showFileMsg("Cannot delete opened file.");
        return;
    }

    _msg = Msg::ConfirmDelete;
    showFileYesNo(kTagConfirmDelete, "Delete file?");
}

void BrowserPage::commitDelete() noexcept
{
    if (!mBrowser.removeSelected()) {
        showFsError();
        return;
    }

    _msg = Msg::None;
    _pending = Pending::DeleteDoneMsg;
}

void BrowserPage::onMsgBox(const nex::msg::evMsgBox& e)
{
    if (e.action == nex::msg::evMsgBox::Action::Yes) {
        switch (_msg) {
        case Msg::OverwriteSave:
            commitSaveAs(_pendingPath);
            return;
        case Msg::ConfirmDelete:
            commitDelete();
            return;
        default:
            break;
        }
    }

    _msg = Msg::None;
    _pendingPath[0] = '\0';
    clearFileRowSelection();
    redrawRows();
}

} // namespace server
