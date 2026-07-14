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
    : Page<37>(app, HMI_COMP_OBJNAME(browser), PageCfg::kPageId)
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
    mBrowser.clearSelection();
    _pending = Pending::ReadMode;
    mode.val.get();
}

void BrowserPage::onTouch(const nex::msg::evTouch& e)
{
    if (e.state != nex::TouchState::Release) {
        return;
    }

    using PB = nex::hmi::Page_browser;
    const uint8_t comp = e.route.comp;

    if (comp == PB::bFNext) {
        onPageNext();
        return;
    }
    if (comp == PB::bFPrev) {
        onPagePrev();
        return;
    }
    if (comp == PB::bAction) {
        onAction();
        return;
    }

    if (comp >= PB::bF0 && comp <= PB::bF7) {
        const std::size_t row = static_cast<std::size_t>(comp - PB::bF0);
        onFileRow(row);
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
    applyModeLayout();
    refreshList();
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

void BrowserPage::applyModeLayout() noexcept
{
    mBrowser.setPageRows(visibleRows());
}

void BrowserPage::refreshList() noexcept
{
    applyModeLayout();
    (void)mBrowser.refresh();
    redrawRows();
}

void BrowserPage::redrawRows() noexcept
{
    char stamp[20]{};
    const std::size_t rows = visibleRows();

    for (std::size_t i = 0; i < FileRows::kCount; ++i) {
        if (i >= rows) {
            fileRows[i].setText("");
            fileDates[i].setText("");
            continue;
        }

        const MBrowser::Entry* entry = mBrowser.entryAt(i);
        if (entry == nullptr) {
            fileRows[i].setText("");
            fileDates[i].setText("");
            continue;
        }

        fileRows[i].setText(entry->name);
        formatFatStamp(stamp, sizeof(stamp), entry->date, entry->time);
        fileDates[i].setText(stamp);
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

void BrowserPage::onPageNext() noexcept
{
    if (mBrowser.pageNext()) {
        mBrowser.clearSelection();
        redrawRows();
    }
}

void BrowserPage::onPagePrev() noexcept
{
    if (mBrowser.pagePrev()) {
        mBrowser.clearSelection();
        redrawRows();
    }
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
        return;
    }
    if (!console.loadShow(path)) {
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
    const char* name = *fNameStr.txt;
    if (name == nullptr || name[0] == '\0') {
        return;
    }

    char path[48]{};
    if (!mBrowser.makePath(path, sizeof(path), name)) {
        return;
    }
    if (!console.saveShow(path)) {
        return;
    }

    refreshList();
    ui().switchPage(ui().work);
}

void BrowserPage::doDelete() noexcept
{
    if (!mBrowser.removeSelected()) {
        return;
    }
    redrawRows();
}

} // namespace server
