#include "browserPage.hpp"

#include "UI/application.hpp"
#include "UI/uiMessages.hpp"

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
    const char* base = smcp::file::FIOManager::showBaseName(name);
    return base != nullptr && std::strncmp(base, kTemplate, sizeof(kTemplate) - 1u) == 0;
}

} // namespace

BrowserPage::BrowserPage(nex::IAppUI& app) noexcept
    : Page<37>(app, HMI_COMP_OBJNAME(browser), PG::kPageId)
{}

void BrowserPage::enterSaveAs() noexcept
{
    _forceSaveAs = true;
    mode.val = static_cast<int32_t>(Mode::SaveAs);
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
    return (currentMode() == Mode::SaveAs) ? 7u : kPageSize;
}

void BrowserPage::onLoad()
{
    clearFileRowSelection();

    if (_forceSaveAs) {
        _forceSaveAs = false;
        _pending = Pending::None;
        _page = 0u;
        if (!console.browser.refresh()) {
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
    _page = 0u;
    if (!console.browser.refresh()) {
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
    _selected = npos;
    BrowserBtn::active_id = 0xFFu;
}

void BrowserPage::updateStatusTexts() noexcept
{
    const std::size_t count = console.browser.cacheCount();
    const std::size_t rows = visibleRows();
    const std::size_t pages = (count == 0u) ? 1u : ((count + rows - 1u) / rows);

    char buf[12]{};
    std::snprintf(buf, sizeof(buf), "%u/%u",
        static_cast<unsigned>(_page + 1u),
        static_cast<unsigned>(pages));
    tfPage.txt.set(buf);

    std::snprintf(buf, sizeof(buf), "%u",
        static_cast<unsigned>(console.browser.dirCount()));
    tfNum.txt.set(buf);
}

void BrowserPage::redrawRows() noexcept
{
    char stamp[20]{};
    const std::size_t rows = visibleRows();
    const std::size_t count = console.browser.cacheCount();
    const std::size_t base = _page * rows;

    if (_selected == npos) {
        clearFileRowSelection();
    }

    for (std::size_t i = 0; i < FileRows::kCount; ++i) {
        using BtnState = BrowserBtn::State;

        if (i >= rows) {
            fileRows[i].hide();
            continue;
        }

        const std::size_t index = base + i;
        const auto* entry = (index < count) ? console.browser.at(static_cast<uint16_t>(index)) : nullptr;
        if (entry == nullptr) {
            fileRows[i].setText("");
            fileDates[i].txt.set("");
            fileRows[i].setState(BtnState::Disabled);
            continue;
        }

        fileRows[i].setText("  ");
        fileRows[i].appendText(entry->name);
        fileRows[i].appendText("\r ");
        const bool rowSelected = (_selected != npos) && (_selected == index);
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
    const std::size_t index = _page * visibleRows() + row;
    if (index >= console.browser.cacheCount()) {
        return;
    }
    _selected = index;

    if (currentMode() == Mode::SaveAs) {
        const auto* entry = console.browser.at(static_cast<uint16_t>(index));
        if (entry != nullptr && entry->name[0] != '\0') {
            fNameStr.txt.set(entry->name);
        }
    }
    redrawRows();
}

void BrowserPage::changePage(bool next) noexcept
{
    const std::size_t count = console.browser.cacheCount();
    const std::size_t rows = visibleRows();
    const std::size_t pages = (count == 0u) ? 1u : ((count + rows - 1u) / rows);
    if (next) {
        if (_page + 1u >= pages) {
            return;
        }
        ++_page;
    } else {
        if (_page == 0u) {
            return;
        }
        --_page;
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
    /* Есть несохранённые правки — спросить, иначе сразу openShow. */
    if (console.show.isEdited()) {
        _msg = Msg::ConfirmDiscardOpen;
        ui().showFileYesNo(kTagDiscardOpen, uiMsg::kConfirmOpenDiscard);
        return;
    }

    commitOpen();
}

void BrowserPage::commitOpen() noexcept
{
    if (_selected == npos) {
        ui().showFileMsg(0u, uiMsg::kBrowserNoSelection);
        return;
    }
    const auto* entry = console.browser.at(static_cast<uint16_t>(_selected));
    if (entry == nullptr || entry->name[0] == '\0') {
        ui().showFileMsg(0u, uiMsg::kBrowserNoSelection);
        return;
    }
    if (!console.fio.openShow(entry->name)) {
        ui().showFileSystemStatus();
        return;
    }
    _msg = Msg::None;
    ui().goWork(true);
}

void BrowserPage::beginSaveAs() noexcept
{
    /* Имя с панели: get → onResponse(getString) → finishSaveAs. */
    _pending = Pending::SaveAsName;
    fNameStr.txt.get();
}

void BrowserPage::finishSaveAs() noexcept
{
    const char* name = fNameStr.txt;
    std::strncpy(_saveAsName, (name != nullptr) ? name : "", sizeof(_saveAsName) - 1u);
    _saveAsName[sizeof(_saveAsName) - 1u] = '\0';

    if (isTemplateName(_saveAsName)) {
        ui().showFileMsg(0u, uiMsg::kConsoleTemplateProtected);
        return;
    }
    if (!console.fio.saveShowAs(_saveAsName)) {
        if (console.fio.status() == smcp::file::FIOManager::Status::BrowserFail
            && console.browser.status() == smcp::file::IBrowser::Status::FileExists) {
            _msg = Msg::OverwriteSave;
            ui().showFileYesNo(kTagOverwriteSave, uiMsg::kConfirmOverwriteFile);
            return;
        }
        ui().showFileSystemStatus();
        return;
    }

    afterSaveAsOk();
}

void BrowserPage::commitSaveAs() noexcept
{
    if (!console.fio.saveShowAs(_saveAsName, true)) {
        ui().showFileSystemStatus();
        return;
    }
    afterSaveAsOk();
}

void BrowserPage::afterSaveAsOk() noexcept
{
    _msg = Msg::None;
    _page = 0u;
    if (!console.browser.refresh()) {
        ui().showBrowserStatus();
        return;
    }
    redrawRows();
    ui().goWork();
}

void BrowserPage::doDelete() noexcept
{
    if (_selected == npos) {
        ui().showFileMsg(0u, uiMsg::kBrowserNoSelection);
        return;
    }
    const auto* entry = console.browser.at(static_cast<uint16_t>(_selected));
    if (entry == nullptr || entry->name[0] == '\0') {
        ui().showFileMsg(0u, uiMsg::kBrowserNoSelection);
        return;
    }
    if (isTemplateName(entry->name)) {
        ui().showFileMsg(0u, uiMsg::kConsoleTemplateProtected);
        return;
    }

    _msg = Msg::ConfirmDelete;
    ui().showFileYesNo(kTagConfirmDelete, uiMsg::kConfirmDeleteFile);
}

void BrowserPage::commitDelete() noexcept
{
    if (_selected == npos) {
        ui().showBrowserStatus();
        return;
    }
    const auto* entry = console.browser.at(static_cast<uint16_t>(_selected));
    if (entry == nullptr) {
        ui().showBrowserStatus();
        return;
    }
    if (!console.fio.removeShow(entry->name)) {
        ui().showFileSystemStatus();
        return;
    }

    _msg = Msg::None;
    /* Список — в onAfterMsgBox после OK на «Файл удалён» (без redraw под вторым MsgBox). */
    ui().showFileMsg(0u, uiMsg::kFileDeleted);
}

void BrowserPage::onMsgBox(const nex::msg::evMsgBox& e)
{
    /* Cancel / OK / No — сброс pending; список в onAfterMsgBox (после refreshPage). */
    if (e.action != nex::msg::evMsgBox::Action::Yes) {
        _msg = Msg::None;
        return;
    }

    /* Yes — продолжить отложенное действие по _msg (может открыть новый MsgBox). */
    switch (_msg) {
    case Msg::OverwriteSave:
        commitSaveAs();
        break;
    case Msg::ConfirmDelete:
        commitDelete();
        break;
    case Msg::ConfirmDiscardOpen:
        commitOpen();
        break;
    case Msg::None:
    default:
        break;
    }
}

void BrowserPage::onAfterMsgBox(const nex::msg::evMsgBox& e)
{
    if (e.action == nex::msg::evMsgBox::Action::Yes) {
        return;
    }
    if (!console.browser.refresh()) {
        return;
    }
    clearFileRowSelection();
    redrawRows();
}

} // namespace server
