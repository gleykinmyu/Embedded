/**
 * @file browser.cpp
 * @brief IBrowser: mount, cwd, refresh, remove / rename / replaceWith / copy.
 */

#include "smcp/Console/browser.hpp"

#include <cstring>

namespace smcp {
namespace file {

IBrowser::IBrowser(BIF::IVolume& volume, BIF::IDirectory& dir, Entry* entries,
                 uint16_t cacheCapacity) noexcept
    : _volume(volume)
    , _dir(dir)
    , _entries(entries)
    , _cacheCapacity(cacheCapacity)
{}

bool IBrowser::open(const char* path) noexcept
{
    clear();
    if (!ensureMounted()) {
        return false;
    }
    const char* src = path;
    if (src == nullptr || src[0] == '\0') {
        src = _volume.path();
    }
    if (src == nullptr || src[0] == '\0') {
        return fail(Status::NotMounted);
    }
    if (!copyPath(_dirPath, sizeof(_dirPath), src)) {
        return fail(Status::PathTooLong);
    }
    return ok();
}

bool IBrowser::changeDirectory(const char* name) noexcept
{
    clear();
    if (!ensureMounted()) {
        return false;
    }
    if (name == nullptr || name[0] == '\0') {
        return fail(Status::InvalidName);
    }
    if (name[0] == '.' && name[1] == '\0') {
        return ok();
    }
    if (name[0] == '.' && name[1] == '.' && name[2] == '\0') {
        return changeDirectoryUp();
    }
    if (isAbsolute(name)) {
        if (!copyPath(_dirPath, sizeof(_dirPath), name)) {
            return fail(Status::PathTooLong);
        }
        return ok();
    }
    if (!isValidName(name)) {
        return fail(Status::InvalidName);
    }
    char next[kBrowserPathSize]{};
    if (!join(next, sizeof(next), _dirPath, name)) {
        return fail(Status::PathTooLong);
    }
    if (!copyPath(_dirPath, sizeof(_dirPath), next)) {
        return fail(Status::PathTooLong);
    }
    return ok();
}

bool IBrowser::changeDirectoryUp() noexcept
{
    clear();
    char* const slash = std::strrchr(_dirPath, '/');
    if (slash == nullptr || _dirPath[0] == '\0') {
        return ok();
    }
    if (slash == _dirPath + 2 && _dirPath[1] == ':') {
        slash[1] = '\0';
        return ok();
    }
    *slash = '\0';
    return ok();
}

bool IBrowser::refresh() noexcept
{
    clear();
    if (_entries == nullptr || _cacheCapacity == 0u) {
        return fail(Status::IoError);
    }
    if (!ensureMounted()) {
        return false;
    }
    if (_dirPath[0] == '\0') {
        if (!open(nullptr)) {
            return false;
        }
    }

    if (!_dir.open(_dirPath)) {
        if (!_volume.remount() || !_dir.open(_dirPath)) {
            return fail(Status::OpenDirFailed);
        }
    }

    Entry entry{};
    while (_dir.next(entry)) {
        if (skip(entry)) {
            continue;
        }
        if (_cacheCount < _cacheCapacity) {
            _entries[_cacheCount++] = entry;
        }
        if (_dirCount < 0xFFFFu) {
            ++_dirCount;
        }
    }
    _dir.close();
    return ok();
}

bool IBrowser::makePath(char* out, std::size_t outLen, const char* name) noexcept
{
    if (!isValidName(name)) {
        return fail(Status::InvalidName);
    }
    if (!join(out, outLen, _dirPath, name)) {
        return fail(Status::PathTooLong);
    }
    return ok();
}

bool IBrowser::contains(const char* name) noexcept
{
    if (name == nullptr || name[0] == '\0') {
        fail(Status::InvalidName);
        return false;
    }
    for (uint16_t i = 0; i < _cacheCount; ++i) {
        if (std::strcmp(_entries[i].name, name) == 0) {
            fail(Status::FileExists);
            return true;
        }
    }
    return false;
}

bool IBrowser::remove(const char* name) noexcept
{
    char path[kBrowserPathSize]{};
    if (!resolvePath(path, sizeof(path), name)) {
        return false;
    }
    if (!_volume.exists(path)) {
        return fail(Status::NotFound);
    }
    if (!_volume.remove(path)) {
        return fail(Status::IoError);
    }
    return refresh();
}

bool IBrowser::rename(const char* from, const char* to) noexcept
{
    if (from != nullptr && to != nullptr && std::strcmp(from, to) == 0) {
        return ok();
    }
    char src[kBrowserPathSize]{};
    char dst[kBrowserPathSize]{};
    if (!resolvePath(src, sizeof(src), from) || !resolvePath(dst, sizeof(dst), to)) {
        return false;
    }
    if (!_volume.exists(src)) {
        return fail(Status::NotFound);
    }
    if (_volume.exists(dst)) {
        return fail(Status::FileExists);
    }
    if (!_volume.rename(src, dst)) {
        return fail(Status::IoError);
    }
    return refresh();
}

bool IBrowser::replaceWith(const char* tmpName, const char* destName) noexcept
{
    if (tmpName == nullptr || destName == nullptr || tmpName[0] == '\0'
        || destName[0] == '\0') {
        return fail(Status::InvalidName);
    }
    if (std::strcmp(tmpName, destName) == 0) {
        return ok();
    }
    char tmpPath[kBrowserPathSize]{};
    char destPath[kBrowserPathSize]{};
    if (!resolvePath(tmpPath, sizeof(tmpPath), tmpName)
        || !resolvePath(destPath, sizeof(destPath), destName)) {
        return false;
    }
    if (!_volume.exists(tmpPath)) {
        return fail(Status::NotFound);
    }
    if (_volume.exists(destPath) && !_volume.remove(destPath)) {
        return fail(Status::IoError);
    }
    if (!_volume.rename(tmpPath, destPath)) {
        (void)_volume.remove(tmpPath);
        return fail(Status::IoError);
    }
    return refresh();
}

bool IBrowser::copy(const char* from, const char* to, BIF::IFile& src, BIF::IFile& dst) noexcept
{
    if (&src == &dst || (from != nullptr && to != nullptr && std::strcmp(from, to) == 0)) {
        return fail(Status::InvalidName);
    }
    char srcPath[kBrowserPathSize]{};
    char dstPath[kBrowserPathSize]{};
    if (!resolvePath(srcPath, sizeof(srcPath), from)
        || !resolvePath(dstPath, sizeof(dstPath), to)) {
        return false;
    }
    if (!_volume.exists(srcPath)) {
        return fail(Status::NotFound);
    }
    if (_volume.exists(dstPath)) {
        return fail(Status::FileExists);
    }

    src.close();
    dst.close();
    if (!src.open(srcPath, false)) {
        return fail(Status::IoError);
    }
    if (!dst.open(dstPath, true)) {
        src.close();
        return fail(Status::IoError);
    }

    uint8_t buf[256]{};
    std::size_t left = src.size();
    bool okCopy = true;
    while (left > 0u) {
        const std::size_t n = (left < sizeof(buf)) ? left : sizeof(buf);
        if (!src.read(buf, n) || !dst.write(buf, n)) {
            okCopy = false;
            break;
        }
        left -= n;
    }
    if (okCopy) {
        okCopy = dst.sync();
    }
    src.close();
    dst.close();
    if (!okCopy) {
        (void)_volume.remove(dstPath);
        return fail(Status::IoError);
    }
    return refresh();
}

bool IBrowser::readyDir() noexcept
{
    if (!ensureMounted()) {
        return false;
    }
    if (_dirPath[0] == '\0') {
        return open(nullptr);
    }
    return true;
}

bool IBrowser::resolvePath(char* out, std::size_t outLen, const char* name) noexcept
{
    if (!readyDir()) {
        return false;
    }
    if (!isValidName(name)) {
        return fail(Status::InvalidName);
    }
    if (!join(out, outLen, _dirPath, name)) {
        return fail(Status::PathTooLong);
    }
    return true;
}

bool IBrowser::ensureMounted() noexcept
{
    if (_volume.isMounted() || _volume.ensureMounted()) {
        return true;
    }
    return fail(Status::NotMounted);
}

bool IBrowser::ok() noexcept
{
    _status = Status::Ok;
    return true;
}

bool IBrowser::fail(Status st) noexcept
{
    _status = st;
    return false;
}

void IBrowser::clear() noexcept
{
    if (_entries != nullptr) {
        for (uint16_t i = 0; i < _cacheCapacity; ++i) {
            _entries[i] = {};
        }
    }
    _cacheCount = 0;
    _dirCount = 0;
    _status = Status::Ok;
}

bool IBrowser::skip(const Entry& e) noexcept
{
    if (e.name[0] == '\0') {
        return true;
    }
    if (e.name[0] == '.' && (e.name[1] == '\0' || (e.name[1] == '.' && e.name[2] == '\0'))) {
        return true;
    }
    return false;
}

bool IBrowser::isAbsolute(const char* path) noexcept
{
    return path != nullptr && std::strchr(path, ':') != nullptr;
}

bool IBrowser::isValidName(const char* name) noexcept
{
    if (name == nullptr || name[0] == '\0') {
        return false;
    }
    if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) {
        return false;
    }
    std::size_t len = 0u;
    for (const char* p = name; *p != '\0'; ++p, ++len) {
        if (len >= BIF::kDirNameSize - 1u) {
            return false;
        }
        const auto c = static_cast<unsigned char>(*p);
        if (c < 0x20u || c == 0x7Fu) {
            return false;
        }
        switch (*p) {
        case '/':
        case '\\':
        case ':':
        case '*':
        case '?':
        case '"':
        case '<':
        case '>':
        case '|':
            return false;
        default:
            break;
        }
    }
    return name[len - 1u] != ' ' && name[len - 1u] != '.';
}

bool IBrowser::copyPath(char* out, std::size_t outLen, const char* src) noexcept
{
    if (out == nullptr || outLen == 0u || src == nullptr) {
        return false;
    }
    const std::size_t n = std::strlen(src);
    if (n + 1u > outLen) {
        return false;
    }
    std::memcpy(out, src, n + 1u);
    return true;
}

bool IBrowser::join(char* out, std::size_t outLen, const char* root, const char* name) noexcept
{
    if (out == nullptr || outLen == 0u || root == nullptr || name == nullptr
        || name[0] == '\0') {
        return false;
    }
    const std::size_t rootLen = std::strlen(root);
    const std::size_t nameLen = std::strlen(name);
    const bool needSlash = (rootLen > 0u && root[rootLen - 1u] != '/');
    const std::size_t total = rootLen + (needSlash ? 1u : 0u) + nameLen + 1u;
    if (total > outLen) {
        return false;
    }
    std::memcpy(out, root, rootLen);
    std::size_t pos = rootLen;
    if (needSlash) {
        out[pos++] = '/';
    }
    std::memcpy(out + pos, name, nameLen);
    out[pos + nameLen] = '\0';
    return true;
}

} // namespace file
} // namespace smcp
