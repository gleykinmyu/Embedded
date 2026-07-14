/**
 * @file show_file.hpp
 * @brief Формат шоуфайла SMCP: заголовок, каталог секций, Reader/Writer.
 *
 * Носитель — `smcp::file::IFile` (см. `fs.hpp`).
 * Запись: Writer(io, n, catalog) → open(path) → writeSection… → finalize().
 * Чтение: Reader(io, catalog, capacity) → open(path) → readSection… → close().
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "fs.hpp"

namespace smcp {
namespace file {

inline constexpr uint32_t kMagic = 0x534D4350u; /**< "SMCP". */
inline constexpr uint16_t kVersion = 0x0114u;
inline constexpr std::size_t kShowFileNameSize = 48u;

#pragma pack(push, 1)

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

constexpr std::size_t Header::payloadBase() const noexcept
{
    return sizeof(Header) + static_cast<std::size_t>(section_count) * sizeof(SectionDesc);
}

#pragma pack(pop)

/**
 * Записать payload секции на носитель и заполнить дескриптор для каталога.
 *
 * @param io              Доступ к файлу на носителе (FatFS и т.д.)
 * @param tag             FourCC-тег секции (uint32_t)
 * @param records         Массив записей фиксированного размера
 * @param record_size     sizeof одной записи
 * @param record_count    Число записей
 * @param payload_offset  Абсолютное смещение payload в файле
 * @param out_desc        Заполняется для последующей записи в каталог
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
 *
 * @code
 * SectionDesc catalog[3];
 * Writer writer(io, 3, catalog);
 * if (!writer.open("0:/show.smcp")) { ... }
 * writer.writeSection(kBaseTag, bases, sizeof(BaseRecord), n);
 * writer.writeSection(kDirTag, dirs, sizeof(DirRecord), n);
 * writer.finalize();  // header + catalog + close
 * @endcode
 */
class Writer {
public:
    Writer(IFile& io, uint16_t section_count, SectionDesc* catalog) noexcept
        : _io(io)
        , _catalog(catalog)
        , _sectionCount(section_count)
    {}

    /** Открыть файл на носителе для записи. */
    [[nodiscard]] bool open(const char* path) noexcept
    {
        if (path == nullptr || _catalog == nullptr) {
            return false;
        }
        if (!_io.open(path, true)) {
            return false;
        }

        _fileOpen = true;
        _hdr.section_count = _sectionCount;
        _nextOffset = _hdr.payloadBase();
        return true;
    }

    /** Закрыть файл без записи заголовка (отмена). */
    void close() noexcept
    {
        if (_fileOpen) {
            _io.close();
            _fileOpen = false;
        }
    }

    /** Записать очередную секцию payload и заполнить элемент каталога. */
    [[nodiscard]] bool writeSection(uint32_t tag,
                                    const void* records,
                                    std::size_t record_size,
                                    std::size_t record_count) noexcept
    {
        if (!_fileOpen || _catalog == nullptr || _written >= _sectionCount) {
            return false;
        }

        if (!file::writeSection(_io, tag, records, record_size, record_count, _nextOffset,
                                _catalog[_written])) {
            return false;
        }

        _nextOffset = _catalog[_written].endOffset();
        ++_written;
        return true;
    }

    /** Записать заголовок, каталог и закрыть файл. */
    [[nodiscard]] bool finalize() noexcept
    {
        if (!_fileOpen || _catalog == nullptr || _written != _sectionCount) {
            close();
            return false;
        }

        _hdr.total_size = static_cast<uint32_t>(_nextOffset);

        bool ok = _io.seek(0);
        ok = ok && _io.write(reinterpret_cast<const uint8_t*>(&_hdr), sizeof(_hdr));
        ok = ok && _io.seek(sizeof(Header));
        ok = ok && _io.write(reinterpret_cast<const uint8_t*>(_catalog),
                            static_cast<std::size_t>(_sectionCount) * sizeof(SectionDesc));
        ok = ok && _io.sync();
        close();
        return ok;
    }

    [[nodiscard]] bool isOpen() const noexcept { return _fileOpen; }

    [[nodiscard]] Header& header() noexcept { return _hdr; }
    [[nodiscard]] const Header& header() const noexcept { return _hdr; }
    [[nodiscard]] std::size_t nextOffset() const noexcept { return _nextOffset; }
    [[nodiscard]] uint16_t writtenCount() const noexcept { return _written; }
    [[nodiscard]] const SectionDesc* catalog() const noexcept { return _catalog; }

private:
    IFile& _io;
    Header _hdr{};
    SectionDesc* _catalog;
    uint16_t _sectionCount;
    uint16_t _written = 0;
    std::size_t _nextOffset = 0;
    bool _fileOpen = false;
};

/**
 * Чтение шоуфайла: заголовок и каталог с носителя, payload по дескриптору.
 *
 * @code
 * SectionDesc catalog[8];
 * Reader reader(io, catalog, 8);
 * if (!reader.open("0:/show.smcp")) { ... }
 * reader.readSection(kBaseTag, bases, sizeof(bases));
 * reader.close();
 * @endcode
 */
class Reader {
public:
    Reader(IFile& io, SectionDesc* catalog, uint16_t catalog_capacity) noexcept
        : _io(io)
        , _catalog(catalog)
        , _catalogCapacity(catalog_capacity)
    {}

    /** Открыть файл на носителе и прочитать заголовок с каталогом. */
    [[nodiscard]] bool open(const char* path) noexcept
    {
        if (path == nullptr || _catalog == nullptr) {
            return false;
        }
        if (!_io.open(path, false)) {
            return false;
        }

        _fileOpen = true;
        if (!load()) {
            close();
            return false;
        }
        return true;
    }

    /** Закрыть файл на носителе. */
    void close() noexcept
    {
        if (_fileOpen) {
            _io.close();
            _fileOpen = false;
        }
        _hdr = {};
        _loaded = false;
    }

    /** Прочитать payload секции по индексу в каталоге. */
    [[nodiscard]] bool readSectionAt(std::size_t index, void* buffer, std::size_t buffer_size) noexcept
    {
        if (!_loaded || index >= _hdr.section_count) {
            return false;
        }
        return file::readSection(_io, _catalog[index], buffer, buffer_size);
    }

    /** Прочитать payload секции по тегу. */
    [[nodiscard]] bool readSection(uint32_t tag, void* buffer, std::size_t buffer_size) noexcept
    {
        if (!_loaded) {
            return false;
        }
        const SectionDesc* desc = find(tag);
        if (desc == nullptr) {
            return false;
        }
        return file::readSection(_io, *desc, buffer, buffer_size);
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
            return false;
        }
        if (!_io.read(reinterpret_cast<uint8_t*>(&_hdr), sizeof(_hdr))) {
            return false;
        }
        if (_hdr.magic != kMagic || _hdr.section_count > _catalogCapacity) {
            return false;
        }
        if (!_io.seek(sizeof(Header))) {
            return false;
        }
        if (!_io.read(reinterpret_cast<uint8_t*>(_catalog),
                      static_cast<std::size_t>(_hdr.section_count) * sizeof(SectionDesc))) {
            return false;
        }
        _loaded = true;
        return true;
    }

    IFile& _io;
    Header _hdr{};
    SectionDesc* _catalog;
    uint16_t _catalogCapacity;
    bool _fileOpen = false;
    bool _loaded = false;
};

} // namespace file
} // namespace smcp
