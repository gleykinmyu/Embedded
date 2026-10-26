/**
 * @file fio.cpp
 * @brief Fio: open / save / bak fallback; Overflow без bak.
 */

#include "fio.hpp"

#include <cstring>

namespace sf {
namespace {

constexpr const char kTempBaseName[] = "tmp";

} // namespace

const char* Fio::cstr(Status st) noexcept
{
    switch (st) {
    case Status::Ok: return "Ok";
    case Status::BakOnly: return "BakOnly";
    case Status::MissingSection: return "MissingSection";
    case Status::InvalidData: return "InvalidData";
    case Status::Overflow: return "Overflow";
    case Status::OpenFileProtected: return "OpenFileProtected";
    case Status::BrowserFail: return "BrowserFail";
    case Status::MainFail: return "MainFail";
    case Status::BakFail: return "BakFail";
    default: return "?";
    }
}

bool Fio::fail(Status st) noexcept
{
    _status = st;
    return false;
}

bool Fio::ok(Event ev, Status st) noexcept
{
    _status = st;
    onEvent(ev);
    return true;
}

void Fio::newShow() noexcept
{
    _live.clearData();
    _live.markEdited();
    _status = Status::Ok;
    onEvent(Event::New);
}

bool Fio::useBak() noexcept
{
    if (!load("", TagMask::all, false)) {
        return fail(_status != Status::Ok ? _status : Status::BakFail);
    }
    return ok(Event::Loaded, Status::BakOnly);
}

bool Fio::openShow(const char* name, const TagMask& tags, bool allowTruncate) noexcept
{
    if (!ensureDir()) {
        return false;
    }
    char path[kPathSize]{};
    if (!_browser.makePath(path, sizeof(path), name)) {
        return fail(Status::BrowserFail);
    }
    if (load(path, tags, allowTruncate)) {
        return ok(Event::Loaded);
    }
    /* Файл цел, но не подходит (Overflow / нет required); live не тронут — bak не подменяем. */
    if (_status == Status::Overflow || _live.status() == ::sf::Status::MissingRequired) {
        return false;
    }
    const Status sdErr = _status;
    if (!useBak()) {
        _status = sdErr;
        return false;
    }
    return true;
}

bool Fio::saveShow() noexcept
{
    if (_live.name()[0] != '\0' && ensureDir()) {
        return saveShowAs(showBaseName(_live.name()), true);
    }
    if (!store(_bak, "", Status::BakFail)) {
        return false;
    }
    return ok(Event::Saved, Status::BakOnly);
}

bool Fio::saveShowAs(const char* name, bool confirmed) noexcept
{
    if (!ensureDir()) {
        return false;
    }
    char path[kPathSize]{};
    char tmp[kPathSize]{};
    if (!_browser.makePath(path, sizeof(path), name)
        || !_browser.makePath(tmp, sizeof(tmp), kTempBaseName)) {
        return fail(Status::BrowserFail);
    }
    if (!_browser.refresh()) {
        return fail(Status::BrowserFail);
    }
    if (_browser.contains(name) && !confirmed) {
        return fail(Status::BrowserFail);
    }

    _live.setName(path);
    if (!store(_main, tmp, Status::MainFail)) {
        (void)_browser.remove(kTempBaseName);
        return false;
    }
    if (!_browser.replaceWith(kTempBaseName, name)) {
        return fail(Status::BrowserFail);
    }
    /* LKG на W25 — тот же полный снимок. */
    if (!store(_bak, "", Status::BakFail)) {
        return false;
    }
    return ok(Event::Saved);
}

bool Fio::removeShow(const char* name) noexcept
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
    if (_status != Status::BakOnly) {
        _status = Status::Ok;
    }
    onEvent(Event::Removed);
    return true;
}

const char* Fio::showBaseName(const char* path) noexcept
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

bool Fio::ensureDir() noexcept
{
    if (_browser.dirPath()[0] == '\0' && !_browser.open(nullptr)) {
        return fail(Status::BrowserFail);
    }
    return true;
}

bool Fio::load(const char* path, const TagMask& tags, bool allowTruncate) noexcept
{
    const bool fromMain = (path != nullptr && path[0] != '\0');
    IFile& src = fromMain ? _main : _bak;
    const char* p = fromMain ? path : "";
    const ::sf::Status st = _live.load(src, p, tags, allowTruncate);
    if (st == ::sf::Status::Overflow) {
        return fail(Status::Overflow);
    }
    if (st == ::sf::Status::MissingRequired) {
        return fail(Status::MissingSection);
    }
    if (st != ::sf::Status::Ok) {
        return fail(fromMain ? Status::MainFail : Status::BakFail);
    }
    if (!_live.allRequiredPresent()) {
        return fail(Status::MissingSection);
    }
    if (!_live.isValid()) {
        return fail(Status::InvalidData);
    }
    return true;
}

bool Fio::store(IFile& io, const char* path, Status failSt) noexcept
{
    if (_live.save(io, path) != ::sf::Status::Ok) {
        return fail(failSt);
    }
    return true;
}

} // namespace sf
