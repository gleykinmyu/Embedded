/**
 * @file fiomanager.cpp
 * @brief FIOManager: open / save (tmp+replaceWith) / restore / bak.
 */

#include "smcp/Console/fiomanager.hpp"

#include <cstring>

namespace smcp {
namespace file {
namespace {

constexpr const char kTempBaseName[] = "tmp";

} // namespace

const char* FIOManager::cstr(Status st) noexcept
{
    switch (st) {
    case Status::Ok: return "Ok";
    case Status::NoShowOpen: return "NoShowOpen";
    case Status::MissingSection: return "MissingSection";
    case Status::InvalidData: return "InvalidData";
    case Status::OpenFileProtected: return "OpenFileProtected";
    case Status::BrowserFail: return "BrowserFail";
    case Status::MainFail: return "MainFail";
    case Status::BakFail: return "BakFail";
    case Status::RestoreFail: return "RestoreFail";
    default: return "?";
    }
}

void FIOManager::newShow() noexcept
{
    /* RAM сбрасывается; edited — да: файла на носителе ещё нет. */
    _live.clearData();
    _live.markEdited();
    _status = Status::Ok;
    onEvent(Event::New);
}

bool FIOManager::restore() noexcept
{
    /* Пустой path в loadShows читает bak, не main. */
    if (!loadShows("")) {
        return fail(Status::RestoreFail);
    }
    return acceptLoaded();
}

bool FIOManager::openShow(const char* name) noexcept
{
    if (!ensureDir()) {
        return false;
    }
    char path[kPathSize]{};
    if (!_browser.makePath(path, sizeof(path), name)) {
        return fail(Status::BrowserFail);
    }
    if (!loadShows(path)) {
        return false;
    }
    return acceptLoaded();
}

bool FIOManager::saveShow() noexcept
{
    if (_live.name()[0] == '\0') {
        return fail(Status::NoShowOpen);
    }
    /* confirmed: перезапись своего же файла без диалога FileExists. */
    return saveShowAs(showBaseName(_live.name()), true);
}

bool FIOManager::saveShowAs(const char* name, bool confirmed) noexcept
{
    if (!ensureDir()) {
        return false;
    }
    char path[kPathSize]{};
    if (!_browser.makePath(path, sizeof(path), name)) {
        return fail(Status::BrowserFail);
    }
    char tmp[kPathSize]{};
    if (!_browser.makePath(tmp, sizeof(tmp), kTempBaseName)) {
        return fail(Status::BrowserFail);
    }

    /* Кэш нужен и для contains, и чтобы replaceWith видел актуальный каталог. */
    if (!_browser.refresh()) {
        return fail(Status::BrowserFail);
    }
    if (_browser.contains(name) && !confirmed) {
        return fail(Status::BrowserFail);
    }

    _live.setName(path);
    /* Сначала tmp: при сбое основной файл не урезан. */
    if (!saveMain(tmp)) {
        (void)_browser.remove(kTempBaseName);
        return false;
    }
    if (!_browser.replaceWith(kTempBaseName, name)) {
        return fail(Status::BrowserFail);
    }
    /* Bak после успешной подмены на томе. */
    if (!syncBak()) {
        return false;
    }
    _live.clearEdited();
    return succeed(Event::Saved);
}

bool FIOManager::removeShow(const char* name) noexcept
{
    if (!ensureDir()) {
        return false;
    }
    char path[kPathSize]{};
    if (!_browser.makePath(path, sizeof(path), name)) {
        return fail(Status::BrowserFail);
    }
    /* Нельзя снести файл, который сейчас открыт в live. */
    if (_live.name()[0] != '\0' && std::strcmp(path, _live.name()) == 0) {
        return fail(Status::OpenFileProtected);
    }
    if (!_browser.remove(name)) {
        return fail(Status::BrowserFail);
    }
    return succeed(Event::Removed);
}

const char* FIOManager::showBaseName(const char* path) noexcept
{
    if (path == nullptr || path[0] == '\0') {
        return "";
    }
    const char* base = path;
    for (const char* p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    return base;
}

bool FIOManager::acceptLoaded() noexcept
{
    if (!_live.allRequiredPresent()) {
        return fail(Status::MissingSection);
    }
    if (!_live.isValid()) {
        return fail(Status::InvalidData);
    }
    /* Копия с носителя совпадает с диском. */
    _live.clearEdited();
    return succeed(Event::Loaded);
}

bool FIOManager::ensureDir() noexcept
{
    if (_browser.dirPath()[0] == '\0' && !_browser.open(nullptr)) {
        return fail(Status::BrowserFail);
    }
    return true;
}

bool FIOManager::loadShows(const char* path) noexcept
{
    /* Непустой path — main (SD); пустой — bak (вспышка / тот же IFile). */
    const bool fromMain = (path != nullptr && path[0] != '\0');
    IFile& src = fromMain ? _main : _bak;
    const char* p = fromMain ? path : "";
    const file::Status st = _incoming.load(src, p);
    if (st != file::Status::Ok) {
        return fail(fromMain ? Status::MainFail : Status::BakFail);
    }
    /* Live не трогаем, пока staging не скопировался целиком. */
    if (!_live.copyFrom(_incoming)) {
        return fail(fromMain ? Status::MainFail : Status::BakFail);
    }
    return true;
}

bool FIOManager::saveMain(const char* path) noexcept
{
    const file::Status st = _live.save(_main, path);
    if (st != file::Status::Ok) {
        return fail(Status::MainFail);
    }
    return true;
}

bool FIOManager::syncBak() noexcept
{
    if (!hasBak()) {
        return true;
    }
    const file::Status st = _live.save(_bak, "");
    if (st != file::Status::Ok) {
        return fail(Status::BakFail);
    }
    return true;
}

} // namespace file
} // namespace smcp
