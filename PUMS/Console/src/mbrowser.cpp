#include "mbrowser.hpp"

#include <cstring>

namespace {

bool appendPath(char* out, std::size_t outLen, const char* root, const char* name) noexcept
{
    if (out == nullptr || outLen == 0u || root == nullptr || name == nullptr || name[0] == '\0') {
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

/** Только одно 8.3-имя: без путей, `..`, управляющих и запрещённых символов FAT. */
[[nodiscard]] bool isValidShowBaseName(const char* name) noexcept
{
    if (name == nullptr || name[0] == '\0') {
        return false;
    }
    if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'))) {
        return false;
    }

    std::size_t len = 0u;
    std::size_t dot = static_cast<std::size_t>(-1);
    for (const char* p = name; *p != '\0'; ++p, ++len) {
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
        case '.':
            if (dot != static_cast<std::size_t>(-1)) {
                return false;
            }
            dot = len;
            break;
        default:
            break;
        }
    }

    if (len >= smcp::file::kDirNameSize) {
        return false;
    }
    if (dot == static_cast<std::size_t>(-1)) {
        return len <= 8u;
    }
    if (dot == 0u || dot > 8u) {
        return false;
    }
    const std::size_t ext = len - dot - 1u;
    return ext >= 1u && ext <= 3u;
}

} // namespace

MBrowser::MBrowser(smcp::file::IVolume& volume,
                   smcp::file::IDirectory& dir,
                   smcp::file::IFile& file,
                   const char* root) noexcept
    : _volume(volume)
    , _dir(dir)
    , _file(file)
    , _rootOverride(root)
{}

const char* MBrowser::statusText(Status status) noexcept
{
    switch (status) {
    case Status::Ok:
        return "OK";
    case Status::NotMounted:
        return "SD card not ready.";
    case Status::OpenDirFailed:
        return "Cannot open folder.";
    case Status::InvalidName:
        return "Invalid file name.";
    case Status::NoSelection:
        return "Select a file.";
    case Status::NotFound:
        return "File not found.";
    case Status::FileExists:
        return "File already exists.";
    case Status::PathTooLong:
        return "Path too long.";
    case Status::OpenFileProtected:
        return "Cannot delete opened file.";
    case Status::RemoveFailed:
        return "Cannot delete file.";
    case Status::RenameFailed:
        return "Cannot rename file.";
    case Status::IoError:
    default:
        return "Storage error.";
    }
}

const char* MBrowser::root() const noexcept
{
    if (_rootOverride != nullptr && _rootOverride[0] != '\0') {
        return _rootOverride;
    }
    return _volume.path();
}

void MBrowser::clear() noexcept
{
    for (std::size_t i = 0; i < kMaxFiles; ++i) {
        _files[i] = {};
    }
    _count = 0u;
    _page = 0u;
    _selected = npos;
}

void MBrowser::setPageRows(std::size_t rows) noexcept
{
    if (rows == 0u) {
        rows = 1u;
    }
    if (rows > kPageSize) {
        rows = kPageSize;
    }
    if (_pageRows == rows) {
        return;
    }
    _pageRows = rows;
    if (_page >= pageCount()) {
        _page = pageCount() - 1u;
    }
}

bool MBrowser::refresh() noexcept
{
    clear();
    clearError();

    if (!_volume.ensureMounted()) {
        _status = Status::NotMounted;
        return false;
    }

    const char* const rootPath = root();
    if (rootPath == nullptr || rootPath[0] == '\0') {
        _status = Status::NotMounted;
        return false;
    }

    if (!_dir.open(rootPath)) {
        /* Возможна «залипшая» сессия после ошибки карты — один remount. */
        if (!_volume.remount() || !_dir.open(rootPath)) {
            _status = Status::OpenDirFailed;
            return false;
        }
    }

    smcp::file::DirEntry entry{};
    while (_count < kMaxFiles && _dir.next(entry)) {
        if (entry.isDir() || entry.name[0] == '\0' || entry.name[0] == '.') {
            continue;
        }
        _files[_count++] = entry;
    }

    _dir.close();
    _status = Status::Ok;
    return true;
}

std::size_t MBrowser::pageCount() const noexcept
{
    if (_count == 0u) {
        return 1u;
    }
    return (_count + _pageRows - 1u) / _pageRows;
}

bool MBrowser::setPage(std::size_t page) noexcept
{
    const std::size_t pages = pageCount();
    if (page >= pages) {
        return false;
    }
    if (_page == page) {
        return false;
    }
    _page = page;
    return true;
}

bool MBrowser::pageNext() noexcept
{
    return setPage(_page + 1u);
}

bool MBrowser::pagePrev() noexcept
{
    if (_page == 0u) {
        return false;
    }
    return setPage(_page - 1u);
}

const MBrowser::Entry* MBrowser::entryAt(std::size_t row) const noexcept
{
    if (row >= _pageRows) {
        return nullptr;
    }
    const std::size_t index = _page * _pageRows + row;
    if (index >= _count) {
        return nullptr;
    }
    return &_files[index];
}

const char* MBrowser::nameAt(std::size_t row) const noexcept
{
    const Entry* e = entryAt(row);
    return (e != nullptr) ? e->name : "";
}

const MBrowser::Entry* MBrowser::selected() const noexcept
{
    if (_selected >= _count) {
        return nullptr;
    }
    return &_files[_selected];
}

const char* MBrowser::selectedName() const noexcept
{
    const Entry* e = selected();
    return (e != nullptr) ? e->name : "";
}

bool MBrowser::selectIndex(std::size_t index) noexcept
{
    if (index >= _count) {
        return false;
    }
    _selected = index;
    return true;
}

bool MBrowser::selectRow(std::size_t row) noexcept
{
    if (row >= _pageRows) {
        return false;
    }
    return selectIndex(_page * _pageRows + row);
}

bool MBrowser::makePath(char* out, std::size_t outLen, const char* name) noexcept
{
    clearError();
    if (!isValidShowBaseName(name)) {
        _status = Status::InvalidName;
        return false;
    }
    if (!appendPath(out, outLen, root(), name)) {
        _status = Status::PathTooLong;
        return false;
    }
    _status = Status::Ok;
    return true;
}

bool MBrowser::makeSelectedPath(char* out, std::size_t outLen) noexcept
{
    clearError();
    const char* name = selectedName();
    if (name[0] == '\0') {
        _status = Status::NoSelection;
        return false;
    }
    return makePath(out, outLen, name);
}

bool MBrowser::pathExists(const char* path) noexcept
{
    return path != nullptr && path[0] != '\0' && _volume.exists(path);
}

bool MBrowser::prepareOpenSelected(char* out, std::size_t outLen) noexcept
{
    if (!makeSelectedPath(out, outLen)) {
        return false;
    }
    if (pathExists(out)) {
        _status = Status::Ok;
        return true;
    }

    char name[smcp::file::kDirNameSize]{};
    std::strncpy(name, selectedName(), sizeof(name) - 1u);
    name[sizeof(name) - 1u] = '\0';

    /* Список мог устареть — один refresh, путь тот же. */
    if (!refresh()) {
        return false;
    }
    if (!pathExists(out)) {
        _status = Status::NotFound;
        return false;
    }
    for (std::size_t i = 0; i < _count; ++i) {
        if (std::strcmp(_files[i].name, name) == 0) {
            _selected = i;
            break;
        }
    }
    _status = Status::Ok;
    return true;
}

bool MBrowser::prepareSaveAs(const char* name, char* out, std::size_t outLen) noexcept
{
    if (!makePath(out, outLen, name)) {
        return false;
    }
    if (pathExists(out)) {
        _status = Status::FileExists;
        return false;
    }
    _status = Status::Ok;
    return true;
}

bool MBrowser::removeSelected(const char* protectedPath) noexcept
{
    char path[48]{};
    if (!makeSelectedPath(path, sizeof(path))) {
        return false;
    }
    if (protectedPath != nullptr && protectedPath[0] != '\0'
        && std::strcmp(path, protectedPath) == 0) {
        _status = Status::OpenFileProtected;
        return false;
    }
    if (!_volume.remove(path)) {
        _status = Status::RemoveFailed;
        return false;
    }
    if (!refresh()) {
        return false;
    }
    _status = Status::Ok;
    return true;
}

bool MBrowser::renameSelected(const char* newName) noexcept
{
    if (newName == nullptr || newName[0] == '\0') {
        _status = Status::InvalidName;
        return false;
    }

    char from[48]{};
    char to[48]{};
    if (!makeSelectedPath(from, sizeof(from))) {
        return false;
    }
    if (!makePath(to, sizeof(to), newName)) {
        return false;
    }
    if (!_volume.rename(from, to)) {
        _status = Status::RenameFailed;
        return false;
    }
    if (!refresh()) {
        return false;
    }
    _status = Status::Ok;
    return true;
}
