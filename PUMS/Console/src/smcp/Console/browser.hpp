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
        NotMounted,    /**< Том не смонтирован. */
        OpenDirFailed, /**< Не удалось открыть каталог. */
        InvalidName,   /**< Пустое / запрещённые символы / `.` `..`. */
        PathTooLong,   /**< Путь не влезает в буфер. */
        NotFound,      /**< Имени нет на томе. */
        FileExists,    /**< Цель уже есть (rename / copy / contains). */
        IoError,       /**< Носитель. */
    };

    /** Кэш — внешний буфер Entry[cacheCapacity]. */
    IBrowser(BIF::IVolume& volume, BIF::IDirectory& dir, Entry* entries,
             uint16_t cacheCapacity) noexcept;

    /** Кэш — внешний массив Entry[N]. */
    template <uint16_t N>
    IBrowser(BIF::IVolume& volume, BIF::IDirectory& dir, Entry (&entries)[N]) noexcept
        : IBrowser(volume, dir, entries, N)
    {}

    IBrowser(const IBrowser&) = delete;
    IBrowser& operator=(const IBrowser&) = delete;

    [[nodiscard]] Status status() const noexcept { return _status; }
    /** Текущий каталог (`0:/…`). */
    [[nodiscard]] const char* dirPath() const noexcept { return _dirPath; }
    [[nodiscard]] uint16_t cacheCapacity() const noexcept { return _cacheCapacity; }
    /** Имена в папке (может быть больше кэша). */
    [[nodiscard]] uint16_t dirCount() const noexcept { return _dirCount; }
    /** Сколько записей сейчас в кэше. */
    [[nodiscard]] uint16_t cacheCount() const noexcept { return _cacheCount; }

    /** Запись кэша; вне диапазона — nullptr. */
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
    /** Подняться на уровень выше; корень тома не трогаем. */
    [[nodiscard]] bool changeDirectoryUp() noexcept;

    /**
     * Сканировать текущий каталог в кэш.
     * cacheCount() ≤ cacheCapacity; dirCount() — сколько имён в папке.
     * Remount только если том не смонтирован или open каталога не удался.
     */
    [[nodiscard]] bool refresh() noexcept;

    /** Собрать полный путь: cwd + basename. Пишет InvalidName / PathTooLong. */
    [[nodiscard]] bool makePath(char* out, std::size_t outLen, const char* name) noexcept;
    /** Имя есть в кэше. true → status FileExists. */
    [[nodiscard]] bool contains(const char* name) noexcept;

    /** Удалить файл в текущем каталоге (basename). Затем refresh(). */
    [[nodiscard]] bool remove(const char* name) noexcept;
    /** Переименовать в текущем каталоге. Цель уже есть → FileExists. */
    [[nodiscard]] bool rename(const char* from, const char* to) noexcept;
    /**
     * Подменить dest файлом tmp (оба — basename текущего каталога).
     * dest нет — rename; dest есть — remove, затем rename.
     * rename не удался — tmp удаляется.
     *
     * Содержимое пишет вызывающий, браузер только меняет имена:
     *   browser.makePath(tmpPath, sizeof(tmpPath), "tmp");
     *   live.save(main, tmpPath);
     *   browser.replaceWith("tmp", "foo.smc");
     *   live.save(bak, "");
     */
    [[nodiscard]] bool replaceWith(const char* tmpName, const char* destName) noexcept;
    /**
     * Копировать файл в текущем каталоге.
     * @a src и @a dst — разные IFile (два FIL). Цель уже есть → FileExists.
     */
    [[nodiscard]] bool copy(const char* from, const char* to, BIF::IFile& src,
                            BIF::IFile& dst) noexcept;

private:
    /** Смонтировать том, если ещё нет. */
    [[nodiscard]] bool ensureMounted() noexcept;
    /** Том смонтирован и cwd задан (иначе open корня). */
    [[nodiscard]] bool readyDir() noexcept;
    /** cwd + валидный basename → полный путь. */
    [[nodiscard]] bool resolvePath(char* out, std::size_t outLen, const char* name) noexcept;
    bool ok() noexcept;
    bool fail(Status st) noexcept;
    /** Обнулить кэш и счётчики; status → Ok. */
    void clear() noexcept;

    /** Пустое имя или `.` / `..`. */
    [[nodiscard]] static bool skip(const Entry& e) noexcept;
    /** Есть двоеточие тома (`0:`). */
    [[nodiscard]] static bool isAbsolute(const char* path) noexcept;
    /** Basename без `.` `..` и запрещённых символов FatFs. */
    [[nodiscard]] static bool isValidName(const char* name) noexcept;
    [[nodiscard]] static bool copyPath(char* out, std::size_t outLen, const char* src) noexcept;
    /** Склеить cwd и basename. */
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

public:
    static constexpr uint16_t kCacheCapacity = N;

    /** Кэш — _entries[N]. */
    Browser(BIF::IVolume& volume, BIF::IDirectory& dir) noexcept
        : IBrowser(volume, dir, _entries, N)
    {}

private:
    Entry _entries[N]{};
};

} // namespace file
} // namespace smcp
