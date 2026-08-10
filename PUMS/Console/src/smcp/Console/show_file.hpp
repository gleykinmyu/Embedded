/**
 * @file show_file.hpp
 * @brief Формат шоуфайла SMCP: заголовок, каталог секций, Reader/Writer.
 *
 * Носитель — `BIF::IFile` (см. `iFileSystem.hpp`).
 * Запись: Writer(io, n, catalog) → open(path) → writeSection… → finalize().
 * Чтение: Reader(io, catalog, capacity) → open(path) → readSection… → close().
 *
 * CRC:
 *   body_crc32  — ISO-HDLC CRC32 по байтам [sizeof(Header) .. total_size);
 *   header_crc16 — CRC16-CCITT по заголовку с полем header_crc16 = 0.
 * Имя шоу — Header::name (первое поле заголовка).
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "iFileSystem.hpp"

namespace smcp {
namespace file {

using BIF::IFile;
inline constexpr uint32_t kMagic = 0x534D4350u; /**< "SMCP". */
inline constexpr uint16_t kVersion = 0x0115u;
inline constexpr std::size_t kCrcChunkSize = 64u;

/** Полный путь шоу: basename + запас под префикс тома (`0:/…`). */
inline constexpr std::size_t kPathPrefixRoom = 16u;
inline constexpr std::size_t kPathSize = BIF::kDirNameSize + kPathPrefixRoom;

/** Результат разбора / записи шоуфайла. */
enum class Status : uint8_t {
    Ok = 0,
    IoError,       /**< open/read/write/seek/sync. */
    BadMagic,      /**< Неверная сигнатура. */
    BadVersion,    /**< Несовместимая версия формата. */
    BadHeaderCrc,  /**< Не совпал header_crc16. */
    BadBodyCrc,    /**< Не совпал body_crc32. */
    BadLayout,     /**< Каталог / размеры / пересечения. */
    Truncated,     /**< Файл короче total_size. */
};

[[nodiscard]] inline const char* cstr(Status s) noexcept
{
    switch (s) {
    case Status::Ok: return "Ok";
    case Status::IoError: return "IoError";
    case Status::BadMagic: return "BadMagic";
    case Status::BadVersion: return "BadVersion";
    case Status::BadHeaderCrc: return "BadHeaderCrc";
    case Status::BadBodyCrc: return "BadBodyCrc";
    case Status::BadLayout: return "BadLayout";
    case Status::Truncated: return "Truncated";
    default: return "?";
    }
}

/**
 * Заголовок шоуфайла.
 * Имя пути — первым полем; magic сразу после него.
 */
struct Header {
    char name[kPathSize]{};
    uint32_t magic = kMagic;
    uint16_t version = kVersion;
    uint16_t content_layers = 0;
    uint16_t section_count = 0;
    uint16_t header_crc16 = 0;
    uint32_t body_crc32 = 0;
    uint32_t total_size = 0;

    /** Смещение начала payload-области после заголовка и каталога. */
    [[nodiscard]] constexpr std::size_t payloadBase() const noexcept;
};

static_assert(sizeof(Header) == kPathSize + 20u);
static_assert(alignof(Header) == alignof(uint32_t));

/** Запись каталога: где в файле лежит секция. */
struct SectionDesc {
    uint32_t tag = 0;
    uint32_t offset = 0;
    uint32_t byte_size = 0;
    uint32_t record_count = 0;

    /** Размер payload секции по размеру одной записи. */
    [[nodiscard]] constexpr std::size_t payloadSize(std::size_t record_size) const noexcept
    {
        return record_size * static_cast<std::size_t>(record_count);
    }

    /** Смещение в файле сразу после payload этой секции (для следующей). */
    [[nodiscard]] constexpr std::size_t endOffset() const noexcept
    {
        return static_cast<std::size_t>(offset) + static_cast<std::size_t>(byte_size);
    }
};

static_assert(sizeof(SectionDesc) == 16u);
static_assert(alignof(SectionDesc) == alignof(uint32_t));

constexpr std::size_t Header::payloadBase() const noexcept
{
    return sizeof(Header) + static_cast<std::size_t>(section_count) * sizeof(SectionDesc);
}

[[nodiscard]] uint16_t computeHeaderCrc16(const Header& hdr) noexcept;

/** Продолжить CRC32 по диапазону файла [offset, offset+length). */
[[nodiscard]] bool updateFileCrc32(IFile& io,
                                   std::size_t offset,
                                   std::size_t length,
                                   uint32_t& crc) noexcept;

/** CRC32 диапазона файла [offset, offset+length). */
[[nodiscard]] bool computeFileCrc32(IFile& io,
                                    std::size_t offset,
                                    std::size_t length,
                                    uint32_t& out_crc) noexcept;

/**
 * Записать payload секции на носитель и заполнить дескриптор для каталога.
 */
[[nodiscard]] bool writeSection(IFile& io,
                                uint32_t tag,
                                const void* records,
                                std::size_t record_size,
                                std::size_t record_count,
                                std::size_t payload_offset,
                                SectionDesc& out_desc) noexcept;

/** Прочитать payload секции по дескриптору из каталога. */
[[nodiscard]] bool readSection(IFile& io,
                               const SectionDesc& desc,
                               void* buffer,
                               std::size_t buffer_size) noexcept;

/**
 * Последовательная сборка шоуфайла: смещения считаются автоматически.
 */
class Writer {
public:
    Writer(IFile& io, uint16_t section_count, SectionDesc* catalog) noexcept;

    [[nodiscard]] Status status() const noexcept { return _status; }

    [[nodiscard]] bool open(const char* path) noexcept;
    void close() noexcept;

    [[nodiscard]] bool writeSection(uint32_t tag,
                                    const void* records,
                                    std::size_t record_size,
                                    std::size_t record_count) noexcept;

    /** Скопировать имя шоу в Header::name (до finalize). */
    void setName(const char* name) noexcept;

    [[nodiscard]] bool finalize(bool closeAfter = true) noexcept;

    [[nodiscard]] bool isOpen() const noexcept { return _fileOpen; }

    [[nodiscard]] Header& header() noexcept { return _hdr; }
    [[nodiscard]] const Header& header() const noexcept { return _hdr; }
    [[nodiscard]] std::size_t nextOffset() const noexcept { return _nextOffset; }
    [[nodiscard]] uint16_t writtenCount() const noexcept { return _written; }
    [[nodiscard]] const SectionDesc* catalog() const noexcept { return _catalog; }

private:
    static constexpr uint16_t kMaxCrcSections = 8u;

    IFile& _io;
    Header _hdr{};
    SectionDesc* _catalog;
    uint16_t _sectionCount;
    uint16_t _written = 0;
    std::size_t _nextOffset = 0;
    const void* _payloadPtr[kMaxCrcSections]{};
    std::size_t _payloadLen[kMaxCrcSections]{};
    bool _fileOpen = false;
    Status _status = Status::Ok;
};

/**
 * Чтение шоуфайла: заголовок и каталог с носителя, payload по дескриптору.
 */
class Reader {
public:
    Reader(IFile& io, SectionDesc* catalog, uint16_t catalog_capacity) noexcept;

    [[nodiscard]] Status status() const noexcept { return _status; }

    [[nodiscard]] bool open(const char* path) noexcept;
    void close() noexcept;

    [[nodiscard]] bool readSectionAt(std::size_t index, void* buffer, std::size_t buffer_size) noexcept;
    [[nodiscard]] bool readSection(uint32_t tag, void* buffer, std::size_t buffer_size) noexcept;

    [[nodiscard]] const SectionDesc* find(uint32_t tag) const noexcept;

    [[nodiscard]] const Header& header() const noexcept { return _hdr; }
    [[nodiscard]] uint16_t sectionCount() const noexcept { return _hdr.section_count; }
    [[nodiscard]] const SectionDesc* catalog() const noexcept { return _catalog; }
    [[nodiscard]] bool isOpen() const noexcept { return _fileOpen; }
    [[nodiscard]] bool isLoaded() const noexcept { return _loaded; }

private:
    [[nodiscard]] bool load() noexcept;

    IFile& _io;
    Header _hdr{};
    SectionDesc* _catalog;
    uint16_t _catalogCapacity;
    bool _fileOpen = false;
    bool _loaded = false;
    Status _status = Status::Ok;
};

} // namespace file
} // namespace smcp
