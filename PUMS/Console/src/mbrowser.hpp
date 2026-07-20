/**
 * @file mbrowser.hpp
 * @brief Модель файлового браузера: кэш имён на томе, пагинация под HMI (8 строк).
 *
 * Не знает FatFs напрямую — только BIF::{IVolume,IDirectory,IFile}.
 * IFile хранится здесь; MConsole держит ссылку на MBrowser для openShow/saveShow.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "iFileSystem.hpp"
#include "smcp/show_file.hpp"

class MBrowser {
public:
    /* --- Константы / типы --- */
    static constexpr std::size_t kMaxFiles = 64u;
    static constexpr std::size_t kPageSize = 8u; /**< Максимум строк на странице (bF0..bF7). */
    static constexpr std::size_t kPathSize = smcp::file::kPathSize;
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    using Entry = BIF::DirEntry;

    enum class Status : uint8_t {
        Ok = 0,
        NotMounted,         /**< Том не смонтирован / ensureMounted failed. */
        OpenDirFailed,      /**< Не удалось открыть каталог. */
        InvalidName,        /**< Имя длиннее `_MAX_LFN` / пустое / путь / запрещённые символы. */
        NoSelection,        /**< Нет выбранного файла. */
        NotFound,           /**< Файл не найден на томе. */
        FileExists,         /**< Путь собран, файл уже есть (Save As → overwrite). */
        PathTooLong,        /**< Путь не умещается в буфер. */
        OpenFileProtected,  /**< Удаление текущего открытого шоу запрещено. */
        RemoveFailed,       /**< Удаление не удалось. */
        IoError,            /**< Прочая ошибка носителя. */
    };

    /**
     * @param volume  Том (mount / exists / remove / rename).
     * @param dir     Итератор каталога для refresh().
     * @param file    Файл для load/save шоу.
     * @param root    Каталог списка; nullptr → volume.path() после mount.
     */
    MBrowser(BIF::IVolume& volume,
             BIF::IDirectory& dir,
             BIF::IFile& file,
             const char* root = nullptr) noexcept;

    /* --- Статус --- */
    [[nodiscard]] Status getStatus() const noexcept { return _status; }
    void clearError() noexcept { _status = Status::Ok; }
    [[nodiscard]] static const char* statusText(Status status) noexcept;

    /* --- Каталог --- */
    /** ensureMounted + сканер каталога → кэш файлов, page=0. */
    [[nodiscard]] bool refresh() noexcept;
    void clear() noexcept;
    [[nodiscard]] std::size_t fileCount() const noexcept { return _count; }
    [[nodiscard]] const char* root() const noexcept;

    /* --- Пагинация --- */
    /** Число видимых строк: 8 (open/delete) или 7 (save as). */
    void setPageRows(std::size_t rows) noexcept;
    [[nodiscard]] std::size_t pageRows() const noexcept { return _pageRows; }
    [[nodiscard]] std::size_t pageCount() const noexcept;
    [[nodiscard]] std::size_t page() const noexcept { return _page; }
    /** true — страница изменилась. */
    [[nodiscard]] bool pageNext() noexcept;
    [[nodiscard]] bool pagePrev() noexcept;
    [[nodiscard]] bool setPage(std::size_t page) noexcept;

    /** Индекс в полном кэше: page * pageRows() + row. */
    [[nodiscard]] const Entry* entryAt(std::size_t row) const noexcept;

    /* --- Выбор --- */
    [[nodiscard]] std::size_t selectedIndex() const noexcept { return _selected; }
    [[nodiscard]] const Entry* selected() const noexcept;
    [[nodiscard]] const char* selectedName() const noexcept;
    [[nodiscard]] bool selectIndex(std::size_t index) noexcept;
    [[nodiscard]] bool selectRow(std::size_t row) noexcept;
    void clearSelection() noexcept { _selected = npos; }

    /* --- Пути --- */
    /**
     * Собрать полный путь: root + name → out.
     * @return false при переполнении / пустых аргументах / невалидном имени.
     */
    [[nodiscard]] bool makePath(char* out, std::size_t outLen, const char* name) noexcept;

    /**
     * Путь для Save As.
     * Файл уже есть → out заполнен, Status::FileExists, false.
     */
    [[nodiscard]] bool prepareSaveAs(const char* name, char* out, std::size_t outLen) noexcept;

    /**
     * Путь выбранного файла + проверка существования на томе.
     * Нет файла → refresh и Status::NotFound.
     */
    [[nodiscard]] bool prepareOpenSelected(char* out, std::size_t outLen) noexcept;

    /* --- Операции с файлами --- */
    /** Удалить выбранный; @a protectedPath — полный путь открытого шоу (нельзя стереть). */
    [[nodiscard]] bool removeSelected(const char* protectedPath = nullptr) noexcept;

    /* --- Носитель (для MConsole) --- */
    [[nodiscard]] BIF::IFile& file() noexcept { return _file; }
    [[nodiscard]] const BIF::IFile& file() const noexcept { return _file; }
    [[nodiscard]] BIF::IVolume& volume() noexcept { return _volume; }
    [[nodiscard]] const BIF::IVolume& volume() const noexcept { return _volume; }

private:
    [[nodiscard]] bool makeSelectedPath(char* out, std::size_t outLen) noexcept;

    BIF::IVolume& _volume;
    BIF::IDirectory& _dir;
    BIF::IFile& _file;
    const char* _rootOverride;

    Entry _files[kMaxFiles]{};
    std::size_t _count = 0u;
    std::size_t _page = 0u;
    std::size_t _pageRows = kPageSize;
    std::size_t _selected = npos;
    Status _status = Status::Ok;
};
