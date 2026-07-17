/**
 * @file mbrowser.hpp
 * @brief Модель файлового браузера: кэш имён на томе, пагинация под HMI (8 строк).
 *
 * Не знает FatFs напрямую — только smcp::file::{IVolume,IDirectory,IFile}.
 * IFile хранится здесь; MConsole::loadShow/saveShow берут его через MBrowser&.
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "smcp/fs.hpp"

class MBrowser {
public:
    static constexpr std::size_t kMaxFiles = 64u;
    static constexpr std::size_t kPageSize = 8u; /**< Максимум строк на странице (bF0..bF7). */

    using Entry = smcp::file::DirEntry;

    enum class Status : uint8_t {
        Ok = 0,
        /** Том не смонтирован / ensureMounted failed. */
        NotMounted,
        /** Не удалось открыть каталог. */
        OpenDirFailed,
        /** Имя не 8.3 / пустое / содержит путь. */
        InvalidName,
        /** Нет выбранного файла. */
        NoSelection,
        /** Файл не найден на томе. */
        NotFound,
        /** Путь собран, файл уже есть (Save As → overwrite). */
        FileExists,
        /** Путь не умещается в буфер. */
        PathTooLong,
        /** Удаление текущего открытого шоу запрещено. */
        OpenFileProtected,
        /** Удаление не удалось. */
        RemoveFailed,
        /** Переименование не удалось. */
        RenameFailed,
        /** Прочая ошибка носителя. */
        IoError,
    };

    /**
     * @param volume  Том (mount / exists / remove / rename).
     * @param dir     Итератор каталога для refresh().
     * @param file    Файл для load/save шоу.
     * @param root    Каталог списка; nullptr → volume.path() после mount.
     */
    MBrowser(smcp::file::IVolume& volume,
             smcp::file::IDirectory& dir,
             smcp::file::IFile& file,
             const char* root = nullptr) noexcept;

    /** ensureMounted + сканер каталога → кэш файлов, page=0. */
    [[nodiscard]] bool refresh() noexcept;

    void clear() noexcept;

    [[nodiscard]] Status getStatus() const noexcept { return _status; }
    void clearError() noexcept { _status = Status::Ok; }

    /** Текст для MsgBox (никогда не nullptr). */
    [[nodiscard]] static const char* statusText(Status status) noexcept;

    /** Число видимых строк: 8 (open/delete) или 7 (save as). */
    void setPageRows(std::size_t rows) noexcept;
    [[nodiscard]] std::size_t pageRows() const noexcept { return _pageRows; }

    [[nodiscard]] std::size_t fileCount() const noexcept { return _count; }
    [[nodiscard]] std::size_t pageCount() const noexcept;
    [[nodiscard]] std::size_t page() const noexcept { return _page; }

    /** true — страница изменилась. */
    [[nodiscard]] bool pageNext() noexcept;
    [[nodiscard]] bool pagePrev() noexcept;
    [[nodiscard]] bool setPage(std::size_t page) noexcept;

    /** Индекс в полном кэше: page * pageRows() + row. */
    [[nodiscard]] const Entry* entryAt(std::size_t row) const noexcept;

    /** Имя файла на текущей странице (или ""). */
    [[nodiscard]] const char* nameAt(std::size_t row) const noexcept;

    /** Абсолютный индекс выбранного файла в кэше; npos — нет выбора. */
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    [[nodiscard]] std::size_t selectedIndex() const noexcept { return _selected; }
    [[nodiscard]] const Entry* selected() const noexcept;
    [[nodiscard]] const char* selectedName() const noexcept;

    /** Выбрать по индексу в кэше или по строке страницы. */
    [[nodiscard]] bool selectIndex(std::size_t index) noexcept;
    [[nodiscard]] bool selectRow(std::size_t row) noexcept;
    void clearSelection() noexcept { _selected = npos; }

    /**
     * Собрать полный путь: root + name → out.
     * @return false при переполнении / пустых аргументах / невалидном имени.
     */
    [[nodiscard]] bool makePath(char* out, std::size_t outLen, const char* name) noexcept;

    [[nodiscard]] bool makeSelectedPath(char* out, std::size_t outLen) noexcept;

    /**
     * Путь выбранного файла + проверка существования на томе.
     * Нет файла → refresh и Status::NotFound.
     */
    [[nodiscard]] bool prepareOpenSelected(char* out, std::size_t outLen) noexcept;

    /**
     * Путь для Save As.
     * Файл уже есть → out заполнен, Status::FileExists, false (UI: overwrite).
     */
    [[nodiscard]] bool prepareSaveAs(const char* name, char* out, std::size_t outLen) noexcept;

    /** Удалить выбранный; @a protectedPath — полный путь открытого шоу (нельзя стереть). */
    [[nodiscard]] bool removeSelected(const char* protectedPath = nullptr) noexcept;

    /** Переименовать выбранный → newName (только имя, без пути). */
    [[nodiscard]] bool renameSelected(const char* newName) noexcept;

    [[nodiscard]] smcp::file::IFile& file() noexcept { return _file; }
    [[nodiscard]] const smcp::file::IFile& file() const noexcept { return _file; }

    [[nodiscard]] const char* root() const noexcept;

private:
    [[nodiscard]] bool pathExists(const char* path) noexcept;

    smcp::file::IVolume& _volume;
    smcp::file::IDirectory& _dir;
    smcp::file::IFile& _file;
    const char* _rootOverride;

    Entry _files[kMaxFiles]{};
    std::size_t _count = 0u;
    std::size_t _page = 0u;
    std::size_t _pageRows = kPageSize;
    std::size_t _selected = npos;
    Status _status = Status::Ok;
};
