/**
 * @file show_file.hpp
 * @brief Формат шоуфайла SMCP: заголовок, каталог секций, Reader/Writer.
 *
 * Носитель — `smcp::file::IFile` (см. `fs.hpp`).
 * Запись: Writer(io, n, catalog) → open(path) → writeSection… → finalize().
 * Чтение: Reader(io, catalog, capacity) → open(path) → readSection… → close().
 *
 * CRC:
 *   body_crc32  — ISO-HDLC CRC32 по байтам [sizeof(Header) .. total_size);
 *   header_crc16 — CRC16-CCITT по заголовку с полем header_crc16 = 0.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "crc.hpp"
#include "fs.hpp"

namespace smcp {
namespace file {

inline constexpr uint32_t kMagic = 0x534D4350u; /**< "SMCP". */
inline constexpr uint16_t kVersion = 0x0114u;
inline constexpr std::size_t kShowFileNameSize = 48u;
inline constexpr std::size_t kCrcChunkSize = 64u;

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

/** Заголовок шоуфайла — 32 байта. */
struct Header {
    uint32_t magic = kMagic;
    uint16_t version = kVersion;
    uint16_t content_layers = 0;
    uint16_t section_count = 0;
    uint16_t header_crc16 = 0;
    uint32_t body_crc32 = 0;
    uint32_t total_size = 0;
    uint8_t reserved[12]{};

    /** Смещение начала payload-области после заголовка и каталога. */
    [[nodiscard]] constexpr std::size_t payloadBase() const noexcept;
};

static_assert(sizeof(Header) == 32u);
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

[[nodiscard]] inline uint16_t computeHeaderCrc16(const Header& hdr) noexcept
{
    Header tmp = hdr;
    tmp.header_crc16 = 0;
    return crc16Ccitt(reinterpret_cast<const uint8_t*>(&tmp), sizeof(tmp));
}

/** Продолжить CRC32 по диапазону файла [offset, offset+length). */
[[nodiscard]] inline bool updateFileCrc32(IFile& io,
                                          std::size_t offset,
                                          std::size_t length,
                                          uint32_t& crc) noexcept
{
    if (length == 0u) {
        return true;
    }
    if (!io.seek(offset)) {
        return false;
    }

    uint8_t buf[kCrcChunkSize]{};
    while (length > 0u) {
        const std::size_t n = (length < kCrcChunkSize) ? length : kCrcChunkSize;
        if (!io.read(buf, n)) {
            return false;
        }
        crc = crc32Update(crc, buf, n);
        length -= n;
    }
    return true;
}

/** CRC32 диапазона файла [offset, offset+length). */
[[nodiscard]] inline bool computeFileCrc32(IFile& io,
                                           std::size_t offset,
                                           std::size_t length,
                                           uint32_t& out_crc) noexcept
{
    uint32_t crc = crc32Init();
    if (!updateFileCrc32(io, offset, length, crc)) {
        return false;
    }
    out_crc = crc32Final(crc);
    return true;
}

/**
 * Записать payload секции на носитель и заполнить дескриптор для каталога.
 */
[[nodiscard]] inline bool writeSection(IFile& io,
                                       uint32_t tag,
                                       const void* records,
                                       std::size_t record_size,
                                       std::size_t record_count,
                                       std::size_t payload_offset,
                                       SectionDesc& out_desc) noexcept
{
    if (records == nullptr || record_size == 0 || record_count == 0) {
        return false;
    }

    out_desc.record_count = static_cast<uint32_t>(record_count);
    const std::size_t byte_size = out_desc.payloadSize(record_size);

    if (!io.seek(payload_offset)) {
        return false;
    }

    if (!io.write(static_cast<const uint8_t*>(records), byte_size)) {
        return false;
    }

    out_desc.tag = tag;
    out_desc.offset = static_cast<uint32_t>(payload_offset);
    out_desc.byte_size = static_cast<uint32_t>(byte_size);
    return true;
}

/** Прочитать payload секции по дескриптору из каталога. */
[[nodiscard]] inline bool readSection(IFile& io,
                                      const SectionDesc& desc,
                                      void* buffer,
                                      std::size_t buffer_size) noexcept
{
    if (buffer == nullptr || buffer_size < static_cast<std::size_t>(desc.byte_size)) {
        return false;
    }

    if (!io.seek(static_cast<std::size_t>(desc.offset))) {
        return false;
    }

    return io.read(static_cast<uint8_t*>(buffer), static_cast<std::size_t>(desc.byte_size));
}

/**
 * Последовательная сборка шоуфайла: смещения считаются автоматически.
 */
class Writer {
public:
    Writer(IFile& io, uint16_t section_count, SectionDesc* catalog) noexcept
        : _io(io)
        , _catalog(catalog)
        , _sectionCount(section_count)
    {}

    [[nodiscard]] Status status() const noexcept { return _status; }

    [[nodiscard]] bool open(const char* path) noexcept
    {
        _status = Status::Ok;
        if (path == nullptr || _catalog == nullptr) {
            _status = Status::BadLayout;
            return false;
        }
        if (!_io.open(path, true)) {
            _status = Status::IoError;
            return false;
        }

        _fileOpen = true;
        _hdr = {};
        _hdr.magic = kMagic;
        _hdr.version = kVersion;
        _hdr.section_count = _sectionCount;
        _written = 0;
        _nextOffset = _hdr.payloadBase();
        for (uint16_t i = 0; i < kMaxCrcSections; ++i) {
            _payloadPtr[i] = nullptr;
            _payloadLen[i] = 0u;
        }
        return true;
    }

    void close() noexcept
    {
        if (_fileOpen) {
            _io.close();
            _fileOpen = false;
        }
    }

    [[nodiscard]] bool writeSection(uint32_t tag,
                                    const void* records,
                                    std::size_t record_size,
                                    std::size_t record_count) noexcept
    {
        if (!_fileOpen || _catalog == nullptr || _written >= _sectionCount
            || _written >= kMaxCrcSections) {
            _status = Status::BadLayout;
            return false;
        }

        if (!file::writeSection(_io, tag, records, record_size, record_count, _nextOffset,
                                _catalog[_written])) {
            _status = Status::IoError;
            return false;
        }

        const std::size_t byte_size = _catalog[_written].payloadSize(record_size);
        _payloadPtr[_written] = records;
        _payloadLen[_written] = byte_size;

        _nextOffset = _catalog[_written].endOffset();
        ++_written;
        return true;
    }

    [[nodiscard]] bool finalize() noexcept
    {
        if (!_fileOpen || _catalog == nullptr || _written != _sectionCount) {
            close();
            _status = Status::BadLayout;
            return false;
        }

        _hdr.magic = kMagic;
        _hdr.version = kVersion;
        _hdr.section_count = _sectionCount;
        _hdr.total_size = static_cast<uint32_t>(_nextOffset);
        _hdr.body_crc32 = 0;
        _hdr.header_crc16 = 0;
        std::memset(_hdr.reserved, 0, sizeof(_hdr.reserved));

        const std::size_t catalogBytes =
            static_cast<std::size_t>(_sectionCount) * sizeof(SectionDesc);

        /* body CRC целиком из RAM: catalog || payload₀ || … */
        uint32_t body_crc = crc32Init();
        body_crc = crc32Update(body_crc, reinterpret_cast<const uint8_t*>(_catalog), catalogBytes);
        for (uint16_t i = 0; i < _written; ++i) {
            if (_payloadPtr[i] == nullptr || _payloadLen[i] == 0u) {
                close();
                _status = Status::BadLayout;
                return false;
            }
            body_crc = crc32Update(body_crc, static_cast<const uint8_t*>(_payloadPtr[i]),
                                   _payloadLen[i]);
        }
        _hdr.body_crc32 = crc32Final(body_crc);
        _hdr.header_crc16 = computeHeaderCrc16(_hdr);

        if (!_io.seek(0)) {
            close();
            _status = Status::IoError;
            return false;
        }
        if (!_io.write(reinterpret_cast<const uint8_t*>(&_hdr), sizeof(_hdr))) {
            close();
            _status = Status::IoError;
            return false;
        }
        if (!_io.write(reinterpret_cast<const uint8_t*>(_catalog), catalogBytes)) {
            close();
            _status = Status::IoError;
            return false;
        }
        if (!_io.sync()) {
            close();
            _status = Status::IoError;
            return false;
        }

        close();
        _status = Status::Ok;
        return true;
    }

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
    Reader(IFile& io, SectionDesc* catalog, uint16_t catalog_capacity) noexcept
        : _io(io)
        , _catalog(catalog)
        , _catalogCapacity(catalog_capacity)
    {}

    [[nodiscard]] Status status() const noexcept { return _status; }

    [[nodiscard]] bool open(const char* path) noexcept
    {
        _status = Status::Ok;
        if (path == nullptr || _catalog == nullptr) {
            _status = Status::BadLayout;
            return false;
        }
        if (!_io.open(path, false)) {
            _status = Status::IoError;
            return false;
        }

        _fileOpen = true;
        if (!load()) {
            close();
            return false;
        }
        return true;
    }

    void close() noexcept
    {
        if (_fileOpen) {
            _io.close();
            _fileOpen = false;
        }
        _hdr = {};
        _loaded = false;
    }

    [[nodiscard]] bool readSectionAt(std::size_t index, void* buffer, std::size_t buffer_size) noexcept
    {
        if (!_loaded || index >= _hdr.section_count) {
            _status = Status::BadLayout;
            return false;
        }
        if (!file::readSection(_io, _catalog[index], buffer, buffer_size)) {
            _status = Status::IoError;
            return false;
        }
        return true;
    }

    [[nodiscard]] bool readSection(uint32_t tag, void* buffer, std::size_t buffer_size) noexcept
    {
        if (!_loaded) {
            _status = Status::BadLayout;
            return false;
        }
        const SectionDesc* desc = find(tag);
        if (desc == nullptr) {
            _status = Status::BadLayout;
            return false;
        }
        if (!file::readSection(_io, *desc, buffer, buffer_size)) {
            _status = Status::IoError;
            return false;
        }
        return true;
    }

    [[nodiscard]] const SectionDesc* find(uint32_t tag) const noexcept
    {
        if (!_loaded) {
            return nullptr;
        }
        for (uint16_t i = 0; i < _hdr.section_count; ++i) {
            if (_catalog[i].tag == tag) {
                return &_catalog[i];
            }
        }
        return nullptr;
    }

    [[nodiscard]] const Header& header() const noexcept { return _hdr; }
    [[nodiscard]] uint16_t sectionCount() const noexcept { return _hdr.section_count; }
    [[nodiscard]] const SectionDesc* catalog() const noexcept { return _catalog; }
    [[nodiscard]] bool isOpen() const noexcept { return _fileOpen; }
    [[nodiscard]] bool isLoaded() const noexcept { return _loaded; }

private:
    [[nodiscard]] bool load() noexcept
    {
        if (!_io.seek(0)) {
            _status = Status::IoError;
            return false;
        }
        if (!_io.read(reinterpret_cast<uint8_t*>(&_hdr), sizeof(_hdr))) {
            _status = Status::IoError;
            return false;
        }

        if (_hdr.magic != kMagic) {
            _status = Status::BadMagic;
            return false;
        }
        if (_hdr.version != kVersion) {
            _status = Status::BadVersion;
            return false;
        }
        if (computeHeaderCrc16(_hdr) != _hdr.header_crc16) {
            _status = Status::BadHeaderCrc;
            return false;
        }
        if (_hdr.section_count == 0u || _hdr.section_count > _catalogCapacity) {
            _status = Status::BadLayout;
            return false;
        }

        const std::size_t payload_base = _hdr.payloadBase();
        if (_hdr.total_size < payload_base) {
            _status = Status::BadLayout;
            return false;
        }
        if (_io.size() < static_cast<std::size_t>(_hdr.total_size)) {
            _status = Status::Truncated;
            return false;
        }

        if (!_io.seek(sizeof(Header))) {
            _status = Status::IoError;
            return false;
        }
        if (!_io.read(reinterpret_cast<uint8_t*>(_catalog),
                      static_cast<std::size_t>(_hdr.section_count) * sizeof(SectionDesc))) {
            _status = Status::IoError;
            return false;
        }

        std::size_t cursor = payload_base;
        for (uint16_t i = 0; i < _hdr.section_count; ++i) {
            const SectionDesc& sec = _catalog[i];
            if (sec.tag == 0u || sec.byte_size == 0u || sec.record_count == 0u) {
                _status = Status::BadLayout;
                return false;
            }
            if (sec.offset != cursor) {
                _status = Status::BadLayout;
                return false;
            }
            if (sec.offset > UINT32_MAX - sec.byte_size) {
                _status = Status::BadLayout;
                return false;
            }
            const std::size_t end = sec.endOffset();
            if (end > static_cast<std::size_t>(_hdr.total_size)) {
                _status = Status::BadLayout;
                return false;
            }
            if ((sec.byte_size % sec.record_count) != 0u) {
                _status = Status::BadLayout;
                return false;
            }
            for (uint16_t j = 0; j < i; ++j) {
                if (_catalog[j].tag == sec.tag) {
                    _status = Status::BadLayout;
                    return false;
                }
            }
            cursor = end;
        }
        if (cursor != static_cast<std::size_t>(_hdr.total_size)) {
            _status = Status::BadLayout;
            return false;
        }

        uint32_t body_crc = 0;
        const std::size_t body_len =
            static_cast<std::size_t>(_hdr.total_size) - sizeof(Header);
        if (!computeFileCrc32(_io, sizeof(Header), body_len, body_crc)) {
            _status = Status::IoError;
            return false;
        }
        if (body_crc != _hdr.body_crc32) {
            _status = Status::BadBodyCrc;
            return false;
        }

        _loaded = true;
        _status = Status::Ok;
        return true;
    }

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
