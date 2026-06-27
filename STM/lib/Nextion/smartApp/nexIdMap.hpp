#pragma once

/**
 * Таблица panel `.id` ↔ compiled id (реестр MCU) и NXCI encode/decode (`BlobHeader` magic `NXCI`).
 * Машина Discover — `SmartApp`.
 */

#include <cstddef>
#include <cstdint>

namespace nex {
namespace idmap {

struct Record {
    uint8_t page = 0xFFu;
    /** Слот в ctor / `getComponent` на MCU. */
    uint8_t compiled_id = 0xFFu;
    /** Фактический `.id` на панели после Discover / Flash. */
    uint8_t panel_id = 0xFFu;
};

struct BlobHeader {
    static constexpr uint32_t kMagic = 0x4E584349u; // "NXCI"
    static constexpr uint16_t kVersion = 2u;

    uint32_t magic = kMagic;
    uint16_t version = kVersion;
    uint16_t count = 0u;
};

static constexpr std::size_t kRecordWireSize = 3u;

/** Реестр `Record`; буфер задаёт `TableStorage<N>` или внешний массив. */
struct Table {
    Record* records = nullptr;
    uint16_t capacity = 0u;
    uint16_t count = 0u;

    constexpr Table() noexcept = default;
    constexpr Table(Record* storage, uint16_t record_capacity) noexcept
        : records(storage)
        , capacity(record_capacity)
        , count(0u)
    {}

    [[nodiscard]] bool isBound() const noexcept { return records != nullptr && capacity > 0u; }
    void clear() noexcept { count = 0u; }

    [[nodiscard]] const Record* find(uint8_t page, uint8_t compiled_id) const noexcept;
    [[nodiscard]] Record* find(uint8_t page, uint8_t compiled_id) noexcept;
    bool upsert(uint8_t page, uint8_t compiled_id, uint8_t panel_id) noexcept;

    [[nodiscard]] static constexpr std::size_t maxEncodedSize(uint16_t record_capacity) noexcept {
        return sizeof(BlobHeader) + static_cast<std::size_t>(record_capacity) * kRecordWireSize;
    }

    [[nodiscard]] std::size_t encode(uint8_t* buf, std::size_t cap) const noexcept;
    [[nodiscard]] bool decode(const uint8_t* buf, std::size_t len) noexcept;
};

template<uint16_t MaxRecords>
struct TableStorage {
    Record records[MaxRecords]{};
    Table table{records, MaxRecords};
};

} // namespace idmap
} // namespace nex
