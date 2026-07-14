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

} // namespace

MBrowser::MBrowser(smcp::file::IVolume& volume,
                   smcp::file::IDirectory& dir,
                   const char* root) noexcept
    : _volume(volume)
    , _dir(dir)
    , _rootOverride(root)
{}

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

    if (!_volume.ensureMounted()) {
        return false;
    }

    const char* const rootPath = root();
    if (rootPath == nullptr || rootPath[0] == '\0') {
        return false;
    }

    if (!_dir.open(rootPath)) {
        return false;
    }

    smcp::file::DirEntry entry{};
    while (_count < kMaxFiles && _dir.next(entry)) {
        if (entry.isDir() || entry.name[0] == '\0' || entry.name[0] == '.') {
            continue;
        }
        _files[_count++] = entry;
    }

    _dir.close();
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

bool MBrowser::makePath(char* out, std::size_t outLen, const char* name) const noexcept
{
    return appendPath(out, outLen, root(), name);
}

bool MBrowser::makeSelectedPath(char* out, std::size_t outLen) const noexcept
{
    const char* name = selectedName();
    if (name[0] == '\0') {
        return false;
    }
    return makePath(out, outLen, name);
}

bool MBrowser::removeSelected() noexcept
{
    char path[48]{};
    if (!makeSelectedPath(path, sizeof(path))) {
        return false;
    }
    if (!_volume.remove(path)) {
        return false;
    }
    return refresh();
}

bool MBrowser::renameSelected(const char* newName) noexcept
{
    if (newName == nullptr || newName[0] == '\0') {
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
        return false;
    }
    return refresh();
}
