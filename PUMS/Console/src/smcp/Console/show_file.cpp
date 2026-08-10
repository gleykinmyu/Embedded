/**
 * @file show_file.cpp
 * @brief Реализация формата шоуфайла SMCP (Reader / Writer / CRC).
 */

#include "show_file.hpp"

#include <cstring>

#include "crc.hpp"
#include "debug.hpp"

namespace smcp {
namespace file {

uint16_t computeHeaderCrc16(const Header& hdr) noexcept
{
    Header tmp = hdr;
    tmp.header_crc16 = 0;
    return crc16Ccitt(reinterpret_cast<const uint8_t*>(&tmp), sizeof(tmp));
}

bool updateFileCrc32(IFile& io,
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

bool computeFileCrc32(IFile& io,
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

bool writeSection(IFile& io,
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

bool readSection(IFile& io,
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

Writer::Writer(IFile& io, uint16_t section_count, SectionDesc* catalog) noexcept
    : _io(io)
    , _catalog(catalog)
    , _sectionCount(section_count)
{}

bool Writer::open(const char* path) noexcept
{
    _status = Status::Ok;
    if (path == nullptr || _catalog == nullptr) {
        _status = Status::BadLayout;
        SMCP_DBG("[SMCP] Writer::open fail %s\n", cstr(_status));
        return false;
    }
    if (!_io.open(path, true)) {
        _status = Status::IoError;
        SMCP_DBG("[SMCP] Writer::open \"%s\" fail %s\n", path, cstr(_status));
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
    SMCP_DBG("[SMCP] Writer::open \"%s\" sections=%u payloadBase=%u\n", path,
        static_cast<unsigned>(_sectionCount), static_cast<unsigned>(_nextOffset));
    return true;
}

void Writer::close() noexcept
{
    if (_fileOpen) {
        SMCP_DBG("[SMCP] Writer::close\n");
        _io.close();
        _fileOpen = false;
    }
}

bool Writer::writeSection(uint32_t tag,
                          const void* records,
                          std::size_t record_size,
                          std::size_t record_count) noexcept
{
    if (!_fileOpen || _catalog == nullptr || _written >= _sectionCount
        || _written >= kMaxCrcSections) {
        _status = Status::BadLayout;
        SMCP_DBG("[SMCP] Writer::writeSection tag=0x%08lX fail %s\n",
            static_cast<unsigned long>(tag), cstr(_status));
        return false;
    }

    if (!file::writeSection(_io, tag, records, record_size, record_count, _nextOffset,
                            _catalog[_written])) {
        _status = Status::IoError;
        SMCP_DBG("[SMCP] Writer::writeSection tag=0x%08lX off=%u fail %s\n",
            static_cast<unsigned long>(tag), static_cast<unsigned>(_nextOffset), cstr(_status));
        return false;
    }

    const std::size_t byte_size = _catalog[_written].payloadSize(record_size);
    _payloadPtr[_written] = records;
    _payloadLen[_written] = byte_size;

    SMCP_DBG("[SMCP] Writer::writeSection #%u tag=0x%08lX rec=%u×%u bytes=%u off=%u\n",
        static_cast<unsigned>(_written), static_cast<unsigned long>(tag),
        static_cast<unsigned>(record_count), static_cast<unsigned>(record_size),
        static_cast<unsigned>(byte_size), static_cast<unsigned>(_catalog[_written].offset));

    _nextOffset = _catalog[_written].endOffset();
    ++_written;
    return true;
}

void Writer::setName(const char* name) noexcept
{
    if (name == nullptr) {
        _hdr.name[0] = '\0';
        return;
    }
    std::strncpy(_hdr.name, name, sizeof(_hdr.name) - 1u);
    _hdr.name[sizeof(_hdr.name) - 1u] = '\0';
}

bool Writer::finalize(bool closeAfter) noexcept
{
    if (!_fileOpen || _catalog == nullptr || _written != _sectionCount) {
        close();
        _status = Status::BadLayout;
        SMCP_DBG("[SMCP] Writer::finalize fail %s (written=%u expect=%u)\n", cstr(_status),
            static_cast<unsigned>(_written), static_cast<unsigned>(_sectionCount));
        return false;
    }

    _hdr.magic = kMagic;
    _hdr.version = kVersion;
    _hdr.section_count = _sectionCount;
    _hdr.total_size = static_cast<uint32_t>(_nextOffset);
    _hdr.body_crc32 = 0;
    _hdr.header_crc16 = 0;
    _hdr.name[sizeof(_hdr.name) - 1u] = '\0';

    const std::size_t catalogBytes =
        static_cast<std::size_t>(_sectionCount) * sizeof(SectionDesc);

    /* body CRC целиком из RAM: catalog || payload₀ || … */
    uint32_t body_crc = crc32Init();
    body_crc = crc32Update(body_crc, reinterpret_cast<const uint8_t*>(_catalog), catalogBytes);
    for (uint16_t i = 0; i < _written; ++i) {
        if (_payloadPtr[i] == nullptr || _payloadLen[i] == 0u) {
            close();
            _status = Status::BadLayout;
            SMCP_DBG("[SMCP] Writer::finalize fail %s (payload #%u)\n", cstr(_status),
                static_cast<unsigned>(i));
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
        SMCP_DBG("[SMCP] Writer::finalize seek0 fail %s\n", cstr(_status));
        return false;
    }
    if (!_io.write(reinterpret_cast<const uint8_t*>(&_hdr), sizeof(_hdr))) {
        close();
        _status = Status::IoError;
        SMCP_DBG("[SMCP] Writer::finalize write hdr fail %s\n", cstr(_status));
        return false;
    }
    if (!_io.write(reinterpret_cast<const uint8_t*>(_catalog), catalogBytes)) {
        close();
        _status = Status::IoError;
        SMCP_DBG("[SMCP] Writer::finalize write catalog fail %s\n", cstr(_status));
        return false;
    }
    if (!_io.sync()) {
        close();
        _status = Status::IoError;
        SMCP_DBG("[SMCP] Writer::finalize sync fail %s\n", cstr(_status));
        return false;
    }

    SMCP_DBG("[SMCP] Writer::finalize ok name=\"%s\" total=%u body_crc=0x%08lX hdr_crc=0x%04X\n",
        _hdr.name, static_cast<unsigned>(_hdr.total_size),
        static_cast<unsigned long>(_hdr.body_crc32),
        static_cast<unsigned>(_hdr.header_crc16));

    if (closeAfter) {
        close();
    }
    _status = Status::Ok;
    return true;
}

Reader::Reader(IFile& io, SectionDesc* catalog, uint16_t catalog_capacity) noexcept
    : _io(io)
    , _catalog(catalog)
    , _catalogCapacity(catalog_capacity)
{}

bool Reader::open(const char* path) noexcept
{
    _status = Status::Ok;
    if (path == nullptr || _catalog == nullptr) {
        _status = Status::BadLayout;
        SMCP_DBG("[SMCP] Reader::open fail %s\n", cstr(_status));
        return false;
    }
    if (!_io.open(path, false)) {
        _status = Status::IoError;
        SMCP_DBG("[SMCP] Reader::open \"%s\" fail %s\n", path, cstr(_status));
        return false;
    }

    _fileOpen = true;
    if (!load()) {
        SMCP_DBG("[SMCP] Reader::open \"%s\" load fail %s\n", path, cstr(_status));
        close();
        return false;
    }
    SMCP_DBG("[SMCP] Reader::open \"%s\" ok name=\"%s\" sections=%u total=%u\n", path, _hdr.name,
        static_cast<unsigned>(_hdr.section_count), static_cast<unsigned>(_hdr.total_size));
    return true;
}

void Reader::close() noexcept
{
    if (_fileOpen) {
        SMCP_DBG("[SMCP] Reader::close\n");
        _io.close();
        _fileOpen = false;
    }
    _hdr = {};
    _loaded = false;
}

bool Reader::readSectionAt(std::size_t index, void* buffer, std::size_t buffer_size) noexcept
{
    if (!_loaded || index >= _hdr.section_count) {
        _status = Status::BadLayout;
        SMCP_DBG("[SMCP] Reader::readSectionAt #%u fail %s\n", static_cast<unsigned>(index),
            cstr(_status));
        return false;
    }
    if (!file::readSection(_io, _catalog[index], buffer, buffer_size)) {
        _status = Status::IoError;
        SMCP_DBG("[SMCP] Reader::readSectionAt #%u tag=0x%08lX fail %s\n",
            static_cast<unsigned>(index), static_cast<unsigned long>(_catalog[index].tag),
            cstr(_status));
        return false;
    }
    SMCP_DBG("[SMCP] Reader::readSectionAt #%u tag=0x%08lX bytes=%u\n",
        static_cast<unsigned>(index), static_cast<unsigned long>(_catalog[index].tag),
        static_cast<unsigned>(_catalog[index].byte_size));
    return true;
}

bool Reader::readSection(uint32_t tag, void* buffer, std::size_t buffer_size) noexcept
{
    if (!_loaded) {
        _status = Status::BadLayout;
        SMCP_DBG("[SMCP] Reader::readSection tag=0x%08lX fail %s\n",
            static_cast<unsigned long>(tag), cstr(_status));
        return false;
    }
    const SectionDesc* desc = find(tag);
    if (desc == nullptr) {
        _status = Status::BadLayout;
        SMCP_DBG("[SMCP] Reader::readSection tag=0x%08lX fail %s (not found)\n",
            static_cast<unsigned long>(tag), cstr(_status));
        return false;
    }
    if (!file::readSection(_io, *desc, buffer, buffer_size)) {
        _status = Status::IoError;
        SMCP_DBG("[SMCP] Reader::readSection tag=0x%08lX fail %s\n",
            static_cast<unsigned long>(tag), cstr(_status));
        return false;
    }
    SMCP_DBG("[SMCP] Reader::readSection tag=0x%08lX bytes=%u off=%u\n",
        static_cast<unsigned long>(tag), static_cast<unsigned>(desc->byte_size),
        static_cast<unsigned>(desc->offset));
    return true;
}

const SectionDesc* Reader::find(uint32_t tag) const noexcept
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

bool Reader::load() noexcept
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
        SMCP_DBG("[SMCP] Reader::load sec[%u] tag=0x%08lX off=%u bytes=%u rec=%u\n",
            static_cast<unsigned>(i), static_cast<unsigned long>(sec.tag),
            static_cast<unsigned>(sec.offset), static_cast<unsigned>(sec.byte_size),
            static_cast<unsigned>(sec.record_count));
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

} // namespace file
} // namespace smcp
