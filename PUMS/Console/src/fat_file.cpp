#include "fat_file.hpp"

#include <cstring>

namespace smcp {
namespace file {

bool FatVolume::mount(const char* path, bool force) noexcept
{
    if (path == nullptr || path[0] == '\0') {
        _last = FR_INVALID_DRIVE;
        return false;
    }

    if (_mounted) {
        unmount();
    }

    _path = path;
    _last = f_mount(&_fs, path, force ? 1u : 0u);
    _mounted = (_last == FR_OK);
    return _mounted;
}

void FatVolume::unmount() noexcept
{
    if (!_mounted) {
        return;
    }
    _last = f_mount(nullptr, _path != nullptr ? _path : "", 0u);
    _mounted = false;
}

bool FatVolume::ensureMounted() noexcept
{
    if (_mounted) {
        return true;
    }
    if (_path == nullptr) {
        _last = FR_INVALID_DRIVE;
        return false;
    }
    return mount(_path, true);
}

bool FatVolume::exists(const char* path) noexcept
{
    if (path == nullptr || path[0] == '\0') {
        _last = FR_INVALID_NAME;
        return false;
    }
    FILINFO info{};
    _last = f_stat(path, &info);
    return _last == FR_OK;
}

bool FatVolume::remove(const char* path) noexcept
{
    if (path == nullptr || path[0] == '\0') {
        _last = FR_INVALID_NAME;
        return false;
    }
    _last = f_unlink(path);
    return _last == FR_OK;
}

bool FatVolume::rename(const char* from, const char* to) noexcept
{
    if (from == nullptr || to == nullptr || from[0] == '\0' || to[0] == '\0') {
        _last = FR_INVALID_NAME;
        return false;
    }
    _last = f_rename(from, to);
    return _last == FR_OK;
}

bool FatDirectory::open(const char* path) noexcept
{
    close();

    if (path == nullptr || path[0] == '\0') {
        _last = FR_INVALID_NAME;
        return false;
    }

    _last = f_opendir(&_dir, path);
    _open = (_last == FR_OK);
    return _open;
}

void FatDirectory::close() noexcept
{
    if (!_open) {
        return;
    }
    _last = f_closedir(&_dir);
    _open = false;
}

bool FatDirectory::next(DirEntry& out) noexcept
{
    out = {};
    if (!_open) {
        _last = FR_INVALID_OBJECT;
        return false;
    }

    FILINFO info{};
    _last = f_readdir(&_dir, &info);
    if (_last != FR_OK || info.fname[0] == '\0') {
        return false;
    }

    std::strncpy(out.name, info.fname, kDirNameSize - 1u);
    out.name[kDirNameSize - 1u] = '\0';
    out.size = static_cast<uint32_t>(info.fsize);
    out.date = info.fdate;
    out.time = info.ftime;
    out.attrib = info.fattrib;
    return true;
}

bool FatFile::open(const char* path, bool for_write) noexcept
{
    close();

    if (path == nullptr || path[0] == '\0') {
        _last = FR_INVALID_NAME;
        return false;
    }

    const BYTE mode = for_write
        ? static_cast<BYTE>(FA_WRITE | FA_CREATE_ALWAYS)
        : static_cast<BYTE>(FA_READ);

    _last = f_open(&_fil, path, mode);
    _open = (_last == FR_OK);
    return _open;
}

void FatFile::close() noexcept
{
    if (!_open) {
        return;
    }
    (void)f_sync(&_fil);
    _last = f_close(&_fil);
    _open = false;
}

bool FatFile::read(uint8_t* data, std::size_t size) noexcept
{
    if (!_open || data == nullptr || size == 0u) {
        return false;
    }

    UINT br = 0u;
    _last = f_read(&_fil, data, static_cast<UINT>(size), &br);
    return _last == FR_OK && static_cast<std::size_t>(br) == size;
}

bool FatFile::write(const uint8_t* data, std::size_t size) noexcept
{
    if (!_open || data == nullptr || size == 0u) {
        return false;
    }

    UINT bw = 0u;
    _last = f_write(&_fil, data, static_cast<UINT>(size), &bw);
    return _last == FR_OK && static_cast<std::size_t>(bw) == size;
}

bool FatFile::seek(std::size_t offset) noexcept
{
    if (!_open) {
        return false;
    }

    _last = f_lseek(&_fil, static_cast<FSIZE_t>(offset));
    return _last == FR_OK;
}

bool FatFile::sync() noexcept
{
    if (!_open) {
        return false;
    }
    _last = f_sync(&_fil);
    return _last == FR_OK;
}

std::size_t FatFile::tell() const noexcept
{
    if (!_open) {
        return 0u;
    }
    return static_cast<std::size_t>(f_tell(&_fil));
}

std::size_t FatFile::size() const noexcept
{
    if (!_open) {
        return 0u;
    }
    return static_cast<std::size_t>(f_size(&_fil));
}

} // namespace file
} // namespace smcp
