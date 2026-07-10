/**
 * @file show_file.hpp
 * @brief Формат шоуфайла SMCP: заголовок, каталог секций, интерфейс чтения/записи.
 *
 * Реализация носителя (FatFS и т.д.) подключается через smcp::file::IFile.
 * Запись: Writer(io, n, catalog) → open(path) → writeSection… → finalize().
 * Чтение: Reader(io, catalog, capacity) → open(path) → readSection… → close().
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace smcp {
namespace file {

inline constexpr uint32_t kMagic = 0x534D4350u; /**< "SMCP". */
inline constexpr uint16_t kVersion = 0x0114u;

/** Интерфейс чтения/записи шоуфайла на носителе (реализация — снаружи, например FatFS). */
class IFile {
public:
    virtual ~IFile() = default;

    /** Открыть файл по пути (реализация — FatFS и т.д.). */
    virtual bool open(const char* path, bool for_write) noexcept = 0;

    /** Закрыть файл и освободить ресурс на носителе. */
    virtual void close() noexcept = 0;

    /** Последовательное или произвольное чтение блока данных. */
    virtual bool read(uint8_t* data, std::size_t size) noexcept = 0;

    /** Последовательная или произвольная запись блока данных. */
    virtual bool write(const uint8_t* data, std::size_t size) noexcept = 0;

    /** Абсолютное смещение в файле (нужно для payload и патча заголовка). */
    virtual bool seek_set(std::size_t offset) noexcept = 0;
};

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

    if (!io.seek_set(payload_offset)) {
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

    if (!io.seek_set(static_cast<std::size_t>(desc.offset))) {
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
        : io_(io)
        , catalog_(catalog)
        , section_count_(section_count)
    {}

    /** Открыть файл на носителе для записи. */
    [[nodiscard]] bool open(const char* path) noexcept
    {
        if (path == nullptr || catalog_ == nullptr) {
            return false;
        }
        if (!io_.open(path, true)) {
            return false;
        }

        file_open_ = true;
        hdr_.section_count = section_count_;
        next_offset_ = hdr_.payloadBase();
        return true;
    }

    /** Закрыть файл без записи заголовка (отмена). */
    void close() noexcept
    {
        if (file_open_) {
            io_.close();
            file_open_ = false;
        }
    }

    /** Записать очередную секцию payload и заполнить элемент каталога. */
    [[nodiscard]] bool writeSection(uint32_t tag,
                                    const void* records,
                                    std::size_t record_size,
                                    std::size_t record_count) noexcept
    {
        if (!file_open_ || catalog_ == nullptr || written_ >= section_count_) {
            return false;
        }

        if (!file::writeSection(io_, tag, records, record_size, record_count, next_offset_,
                                catalog_[written_])) {
            return false;
        }

        next_offset_ = catalog_[written_].endOffset();
        ++written_;
        return true;
    }

    /** Записать заголовок, каталог и закрыть файл. */
    [[nodiscard]] bool finalize() noexcept
    {
        if (!file_open_ || catalog_ == nullptr || written_ != section_count_) {
            close();
            return false;
        }

        hdr_.total_size = static_cast<uint32_t>(next_offset_);

        bool ok = io_.seek_set(0);
        ok = ok && io_.write(reinterpret_cast<const uint8_t*>(&hdr_), sizeof(hdr_));
        ok = ok && io_.seek_set(sizeof(Header));
        ok = ok && io_.write(reinterpret_cast<const uint8_t*>(catalog_),
                            static_cast<std::size_t>(section_count_) * sizeof(SectionDesc));
        close();
        return ok;
    }

    [[nodiscard]] bool isOpen() const noexcept { return file_open_; }

    [[nodiscard]] Header& header() noexcept { return hdr_; }
    [[nodiscard]] const Header& header() const noexcept { return hdr_; }
    [[nodiscard]] std::size_t nextOffset() const noexcept { return next_offset_; }
    [[nodiscard]] uint16_t writtenCount() const noexcept { return written_; }
    [[nodiscard]] const SectionDesc* catalog() const noexcept { return catalog_; }

private:
    IFile& io_;
    Header hdr_{};
    SectionDesc* catalog_;
    uint16_t section_count_;
    uint16_t written_ = 0;
    std::size_t next_offset_ = 0;
    bool file_open_ = false;
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
        : io_(io)
        , catalog_(catalog)
        , catalog_capacity_(catalog_capacity)
    {}

    /** Открыть файл на носителе и прочитать заголовок с каталогом. */
    [[nodiscard]] bool open(const char* path) noexcept
    {
        if (path == nullptr || catalog_ == nullptr) {
            return false;
        }
        if (!io_.open(path, false)) {
            return false;
        }

        file_open_ = true;
        if (!load()) {
            close();
            return false;
        }
        return true;
    }

    /** Закрыть файл на носителе. */
    void close() noexcept
    {
        if (file_open_) {
            io_.close();
            file_open_ = false;
        }
        hdr_ = {};
        loaded_ = false;
    }

    /** Прочитать payload секции по индексу в каталоге. */
    [[nodiscard]] bool readSectionAt(std::size_t index, void* buffer, std::size_t buffer_size) noexcept
    {
        if (!loaded_ || index >= hdr_.section_count) {
            return false;
        }
        return file::readSection(io_, catalog_[index], buffer, buffer_size);
    }

    /** Прочитать payload секции по тегу. */
    [[nodiscard]] bool readSection(uint32_t tag, void* buffer, std::size_t buffer_size) noexcept
    {
        if (!loaded_) {
            return false;
        }
        const SectionDesc* desc = find(tag);
        if (desc == nullptr) {
            return false;
        }
        return file::readSection(io_, *desc, buffer, buffer_size);
    }

    [[nodiscard]] const SectionDesc* find(uint32_t tag) const noexcept
    {
        if (!loaded_) {
            return nullptr;
        }
        for (uint16_t i = 0; i < hdr_.section_count; ++i) {
            if (catalog_[i].tag == tag) {
                return &catalog_[i];
            }
        }
        return nullptr;
    }

    [[nodiscard]] const Header& header() const noexcept { return hdr_; }
    [[nodiscard]] uint16_t sectionCount() const noexcept { return hdr_.section_count; }
    [[nodiscard]] const SectionDesc* catalog() const noexcept { return catalog_; }
    [[nodiscard]] bool isOpen() const noexcept { return file_open_; }
    [[nodiscard]] bool isLoaded() const noexcept { return loaded_; }

private:
    [[nodiscard]] bool load() noexcept
    {
        if (!io_.seek_set(0)) {
            return false;
        }
        if (!io_.read(reinterpret_cast<uint8_t*>(&hdr_), sizeof(hdr_))) {
            return false;
        }
        if (hdr_.magic != kMagic || hdr_.section_count > catalog_capacity_) {
            return false;
        }
        if (!io_.seek_set(sizeof(Header))) {
            return false;
        }
        if (!io_.read(reinterpret_cast<uint8_t*>(catalog_),
                      static_cast<std::size_t>(hdr_.section_count) * sizeof(SectionDesc))) {
            return false;
        }
        loaded_ = true;
        return true;
    }

    IFile& io_;
    Header hdr_{};
    SectionDesc* catalog_;
    uint16_t catalog_capacity_;
    bool file_open_ = false;
    bool loaded_ = false;
};

} // namespace file
} // namespace smcp
