#include "model/mbrowser.hpp"

#include "UI/uiMessages.hpp"
#include "board.hpp"
#include "Debug.h"

#include <cstring>
#include <stm32f4xx_hal.h>

namespace {

/** Пауза перед первым HAL_SD_Init (холодный старт карты). */
constexpr uint32_t kSdPowerSettleMs = 250u;
/** Повтор после неудачного mount. */
constexpr uint32_t kSdRetrySettleMs = 150u;

void sdSettleMs(uint32_t ms) noexcept
{
    const uint32_t t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) < ms) {
        board.watchdog.kick();
    }
    board.watchdog.kick();
}

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

/** Одно имя файла (LFN ≤ `_MAX_LFN`): без путей, `..`, запрещённых символов FAT. */
[[nodiscard]] bool isValidShowBaseName(const char* name) noexcept
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

    /* Windows/FAT: имя не должно заканчиваться пробелом или точкой. */
    return name[len - 1u] != ' ' && name[len - 1u] != '.';
}

} // namespace

MBrowser::MBrowser(BIF::IVolume& volume,
                   BIF::IDirectory& dir,
                   BIF::IFile& file,
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
        return uiMsg::kOk;
    case Status::NotMounted:
        return uiMsg::kBrowserNotMounted;
    case Status::OpenDirFailed:
        return uiMsg::kBrowserOpenDirFailed;
    case Status::InvalidName:
        return uiMsg::kBrowserInvalidName;
    case Status::NoSelection:
        return uiMsg::kBrowserNoSelection;
    case Status::NotFound:
        return uiMsg::kBrowserNotFound;
    case Status::FileExists:
        return uiMsg::kBrowserFileExists;
    case Status::PathTooLong:
        return uiMsg::kBrowserPathTooLong;
    case Status::OpenFileProtected:
        return uiMsg::kBrowserOpenProtected;
    case Status::RemoveFailed:
        return uiMsg::kBrowserRemoveFailed;
    case Status::IoError:
    default:
        return uiMsg::kStorageError;
    }
}

const char* MBrowser::root() const noexcept
{
    if (_rootOverride != nullptr && _rootOverride[0] != '\0') {
        return _rootOverride;
    }
    return _volume.path();
}

bool MBrowser::ensureReady() noexcept
{
    if (_volume.isMounted()) {
        return true;
    }

    SD_DBG("SD power settle %lu ms...\n", static_cast<unsigned long>(kSdPowerSettleMs));
    sdSettleMs(kSdPowerSettleMs);

    if (_volume.ensureMounted()) {
        SD_DBG("SD mounted: %s\n", _volume.path() != nullptr ? _volume.path() : "?");
        return true;
    }

    /* Холодный старт: первый Init часто падает — пауза и один remount. */
    SD_DBG("SD mount failed, retry after %lu ms...\n",
        static_cast<unsigned long>(kSdRetrySettleMs));
    sdSettleMs(kSdRetrySettleMs);

    if (_volume.remount()) {
        SD_DBG("SD mounted (retry): %s\n", _volume.path() != nullptr ? _volume.path() : "?");
        return true;
    }

    SD_DBG("SD mount failed\n");
    _status = Status::NotMounted;
    return false;
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

    SD_DBG("browser refresh...\n");
    if (!ensureReady()) {
        return false;
    }

    const char* const rootPath = root();
    if (rootPath == nullptr || rootPath[0] == '\0') {
        _status = Status::NotMounted;
        return false;
    }

    if (!_dir.open(rootPath)) {
        /* Возможна «залипшая» сессия после ошибки карты — один remount. */
        SD_DBG("opendir failed, remount...\n");
        if (!_volume.remount() || !_dir.open(rootPath)) {
            _status = Status::OpenDirFailed;
            return false;
        }
    }

    BIF::DirEntry entry{};
    while (_count < kMaxFiles && _dir.next(entry)) {
        if (entry.isDir() || entry.name[0] == '\0' || entry.name[0] == '.') {
            continue;
        }
        _files[_count++] = entry;
    }

    _dir.close();
    _status = Status::Ok;
    SD_DBG("browser refresh OK: %u files\n", static_cast<unsigned>(_count));
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

bool MBrowser::prepareSaveAs(const char* name, char* out, std::size_t outLen) noexcept
{
    if (!ensureReady()) {
        return false;
    }
    if (!makePath(out, outLen, name)) {
        return false;
    }
    if (out[0] != '\0' && _volume.exists(out)) {
        _status = Status::FileExists;
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

bool MBrowser::prepareOpenSelected(char* out, std::size_t outLen) noexcept
{
    if (!ensureReady()) {
        return false;
    }
    /* Собрать полный путь выбранного имени. */
    if (!makeSelectedPath(out, outLen)) {
        return false;
    }
    if (out[0] != '\0' && _volume.exists(out)) {
        _status = Status::Ok;
        return true;
    }

    /* На томе нет — список мог устареть: один refresh, путь тот же. */
    char name[BIF::kDirNameSize]{};
    std::strncpy(name, selectedName(), sizeof(name) - 1u);
    name[sizeof(name) - 1u] = '\0';

    if (!refresh()) {
        return false;
    }
    if (out[0] == '\0' || !_volume.exists(out)) {
        _status = Status::NotFound;
        return false;
    }
    /* Восстановить выделение по имени после refresh. */
    for (std::size_t i = 0; i < _count; ++i) {
        if (std::strcmp(_files[i].name, name) == 0) {
            _selected = i;
            break;
        }
    }
    _status = Status::Ok;
    return true;
}

bool MBrowser::removeSelected(const char* protectedPath) noexcept
{
    if (!ensureReady()) {
        return false;
    }
    char path[kPathSize]{};
    if (!makeSelectedPath(path, sizeof(path))) {
        return false;
    }
    /* Нельзя стереть файл, который сейчас открыт как шоу. */
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
