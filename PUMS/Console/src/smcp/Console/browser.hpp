/**
 * @file browser.hpp
 * @brief IBrowser — обход каталога в массив Entry; Browser<N> владеет Entry[N].
 *
 * FatFs даёт только последовательный f_readdir.
 * refresh() сканирует текущий каталог и заполняет кэш. Пагинация — снаружи по кэшу.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "iFileSystem.hpp"

namespace smcp {
namespace file {

inline constexpr std::size_t kBrowserPathPrefix = 16u;
inline constexpr std::size_t kBrowserPathSize = BIF::kDirNameSize + kBrowserPathPrefix;

class IBrowser {
public:
    using Entry = BIF::DirEntry;

    enum class Status : uint8_t {
        Ok = 0,
        NotMounted,
        OpenDirFailed,
        InvalidName,
        PathTooLong,
        NotFound,
        FileExists,
        IoError,
    };

    IBrowser(BIF::IVolume& volume, BIF::IDirectory& dir, Entry* entries,
            uint16_t cacheCapacity) noexcept;

    template <uint16_t N>
    IBrowser(BIF::IVolume& volume, BIF::IDirectory& dir, Entry (&entries)[N]) noexcept
        : IBrowser(volume, dir, entries, N)
    {}

    IBrowser(const IBrowser&) = delete;
    IBrowser& operator=(const IBrowser&) = delete;

    [[nodiscard]] Status status() const noexcept { return _status; }
    [[nodiscard]] const char* dirPath() const noexcept { return _dirPath; }
    [[nodiscard]] uint16_t cacheCapacity() const noexcept { return _cacheCapacity; }

    [[nodiscard]] uint16_t dirCount() const noexcept { return _dirCount; }
    [[nodiscard]] uint16_t cacheCount() const noexcept { return _cacheCount; }

    [[nodiscard]] const Entry* at(uint16_t i) const noexcept
    {
        return (i < _cacheCount) ? &_entries[i] : nullptr;
    }

    [[nodiscard]] const Entry& operator[](uint16_t i) const noexcept
    {
        return _entries[(i < _cacheCount) ? i : 0u];
    }

    /** Смонтировать том и выбрать каталог (nullptr → volume.path()). */
    [[nodiscard]] bool open(const char* path = nullptr) noexcept;

    /** Войти в подкаталог (basename) или по абсолютному пути (`0:/…`). */
    [[nodiscard]] bool changeDirectory(const char* name) noexcept;
    [[nodiscard]] bool changeDirectoryUp() noexcept;

    /**
     * Сканировать текущий каталог в кэш.
     * cacheCount() — сколько в массиве (≤ cacheCapacity); dirCount() — сколько имён в папке.
     */
    [[nodiscard]] bool refresh() noexcept;

    [[nodiscard]] bool makePath(char* out, std::size_t outLen, const char* name) const noexcept;

    /** Удалить файл в текущем каталоге (basename). Затем refresh(). */
    [[nodiscard]] bool remove(const char* name) noexcept;

    /** Переименовать в текущем каталоге. Цель уже есть → FileExists. */
    [[nodiscard]] bool rename(const char* from, const char* to) noexcept;

    /**
     * Копировать файл в текущем каталоге.
     * @a src и @a dst — разные IFile (два FIL). Цель уже есть → FileExists.
     */
    [[nodiscard]] bool copy(const char* from, const char* to, BIF::IFile& src,
                            BIF::IFile& dst) noexcept;

private:
    [[nodiscard]] bool ensureMounted() noexcept;
    [[nodiscard]] bool readyDir() noexcept;
    [[nodiscard]] bool resolvePath(char* out, std::size_t outLen, const char* name) noexcept;
    [[nodiscard]] bool ok() noexcept;
    [[nodiscard]] bool fail(Status st) noexcept;
    void clear() noexcept;

    [[nodiscard]] static bool skip(const Entry& e) noexcept;
    [[nodiscard]] static bool isAbsolute(const char* path) noexcept;
    [[nodiscard]] static bool isValidName(const char* name) noexcept;
    [[nodiscard]] static bool copyPath(char* out, std::size_t outLen, const char* src) noexcept;
    [[nodiscard]] static bool join(char* out, std::size_t outLen, const char* root,
                                   const char* name) noexcept;

    BIF::IVolume& _volume;
    BIF::IDirectory& _dir;
    Entry* _entries;
    uint16_t _cacheCapacity;
    char _dirPath[kBrowserPathSize]{};
    uint16_t _dirCount = 0;
    uint16_t _cacheCount = 0;
    Status _status = Status::Ok;
};

/** Браузер с собственным кэшем Entry[N]. */
template <uint16_t N>
class Browser : public IBrowser {
    static_assert(N > 0u, "Browser: N > 0");

    Entry _entries[N]{};

public:
    static constexpr uint16_t kCacheCapacity = N;

    Browser(BIF::IVolume& volume, BIF::IDirectory& dir) noexcept
        : IBrowser(volume, dir, _entries, N)
    {}
};

} // namespace file
} // namespace smcp
