/**
 * @file show_store.cpp
 * @brief ShowStore: open / save (tmp+replaceWith) / restore / bak.
 */

#include "smcp/Console/show_store.hpp"

#include <cstring>

namespace smcp {
namespace file {
namespace {

constexpr const char kTempBaseName[] = "tmp";

} // namespace

bool ShowStore::acceptLoaded() noexcept
{
    if (!_live.allRequiredPresent()) {
        return fail(Status::MissingSection);
    }
    if (!_live.isValid()) {
        return fail(Status::InvalidData);
    }
    _live.clearEdited();
    return ok();
}

bool ShowStore::ensureDir() noexcept
{
    if (_browser.dirPath()[0] == '\0' && !_browser.open(nullptr)) {
        return fail(Status::BrowserFail);
    }
    return true;
}

bool ShowStore::loadShows(const char* path) noexcept
{
    const bool fromMain = (path != nullptr && path[0] != '\0');
    IFile& src = fromMain ? _main : _bak;
    const char* p = fromMain ? path : "";
    const file::Status st = _incoming.load(src, p);
    if (st != file::Status::Ok) {
        return fail(fromMain ? Status::MainFail : Status::BakFail);
    }
    if (!_live.copyFrom(_incoming)) {
        return fail(fromMain ? Status::MainFail : Status::BakFail);
    }
    return true;
}

bool ShowStore::saveMain(const char* path) noexcept
{
    const file::Status st = _live.save(_main, path);
    if (st != file::Status::Ok) {
        return fail(Status::MainFail);
    }
    return true;
}

bool ShowStore::syncBak() noexcept
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

void ShowStore::newShow() noexcept
{
    _live.clearData();
    _live.clearEdited();
}

bool ShowStore::restore() noexcept
{
    if (!loadShows("")) {
        return fail(Status::RestoreFail);
    }
    return acceptLoaded();
}

bool ShowStore::openShow(const char* name) noexcept
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

bool ShowStore::saveShow() noexcept
{
    if (_live.name()[0] == '\0') {
        return fail(Status::NoShowOpen);
    }
    return saveShowAs(showBaseName(_live.name()), true);
}

bool ShowStore::saveShowAs(const char* name, bool confirmed) noexcept
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

    if (!_browser.refresh()) {
        return fail(Status::BrowserFail);
    }
    if (_browser.contains(name) && !confirmed) {
        return fail(Status::BrowserFail);
    }

    _live.setName(path);
    if (!saveMain(tmp)) {
        (void)_browser.remove(kTempBaseName);
        return false;
    }
    if (!_browser.replaceWith(kTempBaseName, name)) {
        return fail(Status::BrowserFail);
    }
    if (!syncBak()) {
        return false;
    }
    _live.clearEdited();
    return ok();
}

bool ShowStore::removeShow(const char* name) noexcept
{
    if (!ensureDir()) {
        return false;
    }
    char path[kPathSize]{};
    if (!_browser.makePath(path, sizeof(path), name)) {
        return fail(Status::BrowserFail);
    }
    if (_live.name()[0] != '\0' && std::strcmp(path, _live.name()) == 0) {
        return fail(Status::OpenFileProtected);
    }
    if (!_browser.remove(name)) {
        return fail(Status::BrowserFail);
    }
    return ok();
}

const char* ShowStore::showBaseName(const char* path) noexcept
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

} // namespace file
} // namespace smcp
