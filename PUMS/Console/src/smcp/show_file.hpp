/**
 * @file show_file.hpp
 * @brief Секции шоуфайла SMCP и интерфейс записи на носитель.
 *
 * Реализация носителя (FatFS и т.д.) подключается через ShowFileWriteSink.
 * Сборка файла: заголовок → каталог секций → payload'ы (writeShowSection).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "types.hpp"

namespace smcp {

inline constexpr uint32_t kShowMagic = 0x534D4350u; /**< "SMCP". */
inline constexpr uint16_t kShowVersion = 0x0114u;

/** Интерфейс записи байтов на Flash / SD (реализация — снаружи, например FatFS). */
struct ShowFileWriteSink {
    void* context = nullptr;

    /** Последовательная или произвольная запись блока данных. */
    bool (*write)(void* context, const uint8_t* data, std::size_t size) noexcept = nullptr;

    /** Абсолютное смещение в файле (нужно для payload и патча заголовка). */
    bool (*seek_set)(void* context, std::size_t offset) noexcept = nullptr;
};

enum class ShowSectionTag : uint32_t {
    MechBase = 0x45534142u,      /**< "BASE" — id, тип, флаги. */
    MechDirection = 0x20524944u, /**< "DIR " — концевики вверх/вниз. */
    Groups = 0x50555247u,        /**< "GRUP" — группы лебёдок. */
    Presets = 0x53455250u,       /**< "PRES" — снимки позиций. */
};

#pragma pack(push, 1)

/** Заголовок шоуфайла — 32 байта. */
struct ShowFileHeader {
    uint32_t magic = kShowMagic;
    uint16_t version = kShowVersion;
    uint16_t content_layers = 0;
    uint16_t section_count = 0;
    uint16_t header_crc16 = 0;
    uint32_t body_crc32 = 0;
    uint32_t total_size = 0;
    char name[12]{};
};

static_assert(sizeof(ShowFileHeader) == 32u);

/** Запись каталога: где в файле лежит секция. */
struct ShowSectionDesc {
    uint32_t tag = 0;
    uint32_t offset = 0;
    uint32_t byte_size = 0;
    uint32_t record_count = 0;
};

static_assert(sizeof(ShowSectionDesc) == 16u);

/** Запись секции BASE. */
struct MechBaseRecord {
    uint16_t id = 0;
    uint8_t type = 0;
    uint8_t config_flags = 0;
};

static_assert(sizeof(MechBaseRecord) == 4u);

/** Запись секции DIR. */
struct MechDirectionRecord {
    uint8_t limit_up_kind = 0;
    uint8_t limit_up_sense = 0;
    uint8_t limit_down_kind = 0;
    uint8_t limit_down_sense = 0;
    uint8_t options = 0;
    uint8_t reserved = 0;
};

static_assert(sizeof(MechDirectionRecord) == 6u);

#pragma pack(pop)

/** Смещение начала payload-области после заголовка и каталога. */
[[nodiscard]] constexpr std::size_t showPayloadBase(std::size_t section_count) noexcept
{
    return sizeof(ShowFileHeader) + section_count * sizeof(ShowSectionDesc);
}

/** Размер payload одной секции. */
[[nodiscard]] constexpr std::size_t showSectionPayloadSize(std::size_t record_size,
                                                           std::size_t record_count) noexcept
{
    return record_size * record_count;
}

/**
 * Записать payload секции на носитель и заполнить дескриптор для каталога.
 *
 * @param sink            Адаптер записи (FatFS и т.д.)
 * @param tag             Тип секции
 * @param records         Массив записей фиксированного размера
 * @param record_size     sizeof одной записи
 * @param record_count    Число записей
 * @param payload_offset  Абсолютное смещение payload в файле
 * @param out_desc        Заполняется для последующей записи в каталог
 */
[[nodiscard]] inline bool writeShowSection(ShowFileWriteSink& sink,
                                           ShowSectionTag tag,
                                           const void* records,
                                           std::size_t record_size,
                                           std::size_t record_count,
                                           std::size_t payload_offset,
                                           ShowSectionDesc& out_desc) noexcept
{
    if (sink.write == nullptr || records == nullptr || record_size == 0 || record_count == 0) {
        return false;
    }

    const std::size_t byte_size = showSectionPayloadSize(record_size, record_count);

    if (sink.seek_set != nullptr) {
        if (!sink.seek_set(sink.context, payload_offset)) {
            return false;
        }
    }

    if (!sink.write(sink.context, static_cast<const uint8_t*>(records), byte_size)) {
        return false;
    }

    out_desc.tag = static_cast<uint32_t>(tag);
    out_desc.offset = static_cast<uint32_t>(payload_offset);
    out_desc.byte_size = static_cast<uint32_t>(byte_size);
    out_desc.record_count = static_cast<uint32_t>(record_count);
    return true;
}

} // namespace smcp
