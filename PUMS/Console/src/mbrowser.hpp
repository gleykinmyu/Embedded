/**
 * @file mbrowser.hpp
 * @brief Модель файлового браузера: кэш имён на томе, пагинация под HMI (8 строк).
 *
 * Не знает FatFs напрямую — только smcp::file::{IVolume,IDirectory,IFile}.
 * Загрузка/сохранение шоу — через MConsole::loadShow/saveShow (IFile привязан при старте).
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

    /**
     * @param volume  Том (mount / exists / remove / rename).
     * @param dir     Итератор каталога для refresh().
     * @param root    Каталог списка; nullptr → volume.path() после mount.
     */
    MBrowser(smcp::file::IVolume& volume,
             smcp::file::IDirectory& dir,
             const char* root = nullptr) noexcept;

    /** ensureMounted + сканер каталога → кэш файлов, page=0. */
    [[nodiscard]] bool refresh() noexcept;

    void clear() noexcept;

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
     * @return false при переполнении / пустых аргументах.
     */
    [[nodiscard]] bool makePath(char* out, std::size_t outLen, const char* name) const noexcept;

    [[nodiscard]] bool makeSelectedPath(char* out, std::size_t outLen) const noexcept;

    /** Удалить выбранный файл на томе и refresh(). */
    [[nodiscard]] bool removeSelected() noexcept;

    /** Переименовать выбранный → newName (только имя, без пути). */
    [[nodiscard]] bool renameSelected(const char* newName) noexcept;

    [[nodiscard]] smcp::file::IVolume& volume() noexcept { return _volume; }
    [[nodiscard]] const smcp::file::IVolume& volume() const noexcept { return _volume; }

    [[nodiscard]] const char* root() const noexcept;

private:
    smcp::file::IVolume& _volume;
    smcp::file::IDirectory& _dir;
    const char* _rootOverride;

    Entry _files[kMaxFiles]{};
    std::size_t _count = 0u;
    std::size_t _page = 0u;
    std::size_t _pageRows = kPageSize;
    std::size_t _selected = npos;
};
