/**
 * @file iFile.hpp
 * @brief Абстракция файла на носителе (байтовый поток). Без FatFs / ffconf.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace BIF {

/** Чтение/запись файла на носителе. */
class IFile {
public:
    virtual ~IFile() = default;

    /** @a for_write — создать заново; read() уже записанных байт тоже разрешён. */
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

} // namespace BIF
