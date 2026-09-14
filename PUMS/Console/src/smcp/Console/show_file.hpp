/**
 * @file show_file.hpp
 * @brief Reader/Writer шоуфайла (устаревает: формат и load/save — show_model.hpp).
 *
 * Носитель — `BIF::IFile`.
 * Запись: Writer(io, n, catalog) → open(path) → writeSection… → finalize().
 * Чтение: Reader(io, catalog, capacity) → open(path) → readSection… → close().
 */

#pragma once

#include "smcp/Console/show_model.hpp"

namespace smcp {
namespace file {

[[nodiscard]] bool updateFileCrc32(IFile& io,
                                   std::size_t offset,
                                   std::size_t length,
                                   uint32_t& crc) noexcept;

[[nodiscard]] bool writeSection(IFile& io,
                                uint32_t tag,
                                const void* records,
                                std::size_t record_size,
                                std::size_t record_count,
                                std::size_t payload_offset,
                                SectionDesc& out_desc) noexcept;

[[nodiscard]] bool readSection(IFile& io,
                               const SectionDesc& desc,
                               void* buffer,
                               std::size_t buffer_size) noexcept;

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
