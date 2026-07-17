/**
 * @file fat_file.hpp
 * @brief FatFs: IVolume / IDirectory / IFile.
 */

#pragma once

#include "smcp/fs.hpp"

extern "C" {
#include "ff.h"
}

namespace smcp {
namespace file {

class FatVolume : public IVolume {
public:
    [[nodiscard]] bool mount(const char* path, bool force = true) noexcept override;
    void unmount() noexcept override;
    [[nodiscard]] bool ensureMounted() noexcept override;
    /** unmount + mount(force) по сохранённому path. */
    [[nodiscard]] bool remount() noexcept override;
    [[nodiscard]] bool isMounted() const noexcept override { return _mounted; }
    [[nodiscard]] const char* path() const noexcept override { return _path; }
    [[nodiscard]] bool exists(const char* path) noexcept override;
    [[nodiscard]] bool remove(const char* path) noexcept override;
    [[nodiscard]] bool rename(const char* from, const char* to) noexcept override;

    [[nodiscard]] FRESULT lastResult() const noexcept { return _last; }

private:
    FATFS _fs{};
    const char* _path = nullptr;
    bool _mounted = false;
    FRESULT _last = FR_INVALID_OBJECT;
};

class FatDirectory : public IDirectory {
public:
    FatDirectory() = default;
    ~FatDirectory() override { close(); }

    FatDirectory(const FatDirectory&) = delete;
    FatDirectory& operator=(const FatDirectory&) = delete;

    [[nodiscard]] bool open(const char* path) noexcept override;
    void close() noexcept override;
    [[nodiscard]] bool next(DirEntry& out) noexcept override;
    [[nodiscard]] bool isOpen() const noexcept override { return _open; }

    [[nodiscard]] FRESULT lastResult() const noexcept { return _last; }

private:
    DIR _dir{};
    bool _open = false;
    FRESULT _last = FR_INVALID_OBJECT;
};

class FatFile : public IFile {
public:
    FatFile() = default;
    ~FatFile() override { close(); }

    FatFile(const FatFile&) = delete;
    FatFile& operator=(const FatFile&) = delete;

    [[nodiscard]] bool open(const char* path, bool for_write) noexcept override;
    void close() noexcept override;
    [[nodiscard]] bool read(uint8_t* data, std::size_t size) noexcept override;
    [[nodiscard]] bool write(const uint8_t* data, std::size_t size) noexcept override;
    [[nodiscard]] bool seek(std::size_t offset) noexcept override;
    [[nodiscard]] bool sync() noexcept override;
    [[nodiscard]] bool isOpen() const noexcept override { return _open; }
    [[nodiscard]] std::size_t tell() const noexcept override;
    [[nodiscard]] std::size_t size() const noexcept override;

    [[nodiscard]] FRESULT lastResult() const noexcept { return _last; }

private:
    FIL _fil{};
    bool _open = false;
    FRESULT _last = FR_INVALID_OBJECT;
};

} // namespace file
} // namespace smcp
