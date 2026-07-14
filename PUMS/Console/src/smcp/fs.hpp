/**
 * @file fs.hpp
 * @brief Абстракция носителя: том, файл, каталог (реализация — FatFs и т.д.).
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace smcp {
namespace file {

inline constexpr std::size_t kDirNameSize = 13u; /**< 8.3 + NUL (`_USE_LFN == 0`). */

/** Запись каталога (метаданные одного имени). */
struct DirEntry {
    static constexpr uint8_t kAttrDir = 0x10u;
    static constexpr uint8_t kAttrArch = 0x20u;

    char name[kDirNameSize]{};
    uint32_t size = 0u;
    uint16_t date = 0u;
    uint16_t time = 0u;
    uint8_t attrib = 0u;

    [[nodiscard]] bool isDir() const noexcept { return (attrib & kAttrDir) != 0u; }
    [[nodiscard]] bool isEmpty() const noexcept { return name[0] == '\0'; }
};

/** Логический том: mount / unmount и операции с именами на томе. */
class IVolume {
public:
    virtual ~IVolume() = default;

    /** Смонтировать том по пути FatFs (`"0:/"`, `board.SD.volumePath()` …). */
    virtual bool mount(const char* path, bool force = true) noexcept = 0;

    /** Отмонтировать том. */
    virtual void unmount() noexcept = 0;

    /** Повторный mount, если том ещё не смонтирован (нужен сохранённый path). */
    virtual bool ensureMounted() noexcept = 0;

    [[nodiscard]] virtual bool isMounted() const noexcept = 0;

    /** Путь тома, переданный в последний успешный `mount`. */
    [[nodiscard]] virtual const char* path() const noexcept = 0;

    /** Файл или каталог существует. */
    [[nodiscard]] virtual bool exists(const char* path) noexcept = 0;

    /** Удалить файл (`f_unlink`). */
    [[nodiscard]] virtual bool remove(const char* path) noexcept = 0;

    /** Переименовать / переместить. */
    [[nodiscard]] virtual bool rename(const char* from, const char* to) noexcept = 0;
};

/** Последовательный обход каталога. */
class IDirectory {
public:
    virtual ~IDirectory() = default;

    virtual bool open(const char* path) noexcept = 0;
    virtual void close() noexcept = 0;

    /**
     * Следующая запись.
     * @return true — `out` заполнен; false — конец каталога или ошибка.
     */
    virtual bool next(DirEntry& out) noexcept = 0;

    [[nodiscard]] virtual bool isOpen() const noexcept = 0;
};

/** Чтение/запись файла на носителе. */
class IFile {
public:
    virtual ~IFile() = default;

    virtual bool open(const char* path, bool for_write) noexcept = 0;
    virtual void close() noexcept = 0;

    virtual bool read(uint8_t* data, std::size_t size) noexcept = 0;
    virtual bool write(const uint8_t* data, std::size_t size) noexcept = 0;
    virtual bool seek(std::size_t offset) noexcept = 0;

    /** Сбросить буферы записи на носитель. */
    virtual bool sync() noexcept = 0;

    [[nodiscard]] virtual bool isOpen() const noexcept = 0;

    /** Текущая позиция курсора. */
    [[nodiscard]] virtual std::size_t tell() const noexcept = 0;

    /** Размер файла (для открытого файла). */
    [[nodiscard]] virtual std::size_t size() const noexcept = 0;
};

} // namespace file
} // namespace smcp
