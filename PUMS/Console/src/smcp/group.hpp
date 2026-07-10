/**
 * @file group.hpp
 * @brief Структуры данных групп штанкетов SMCP v1.5 (блок 3.3).
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "bitmask.hpp"

namespace smcp {

inline constexpr std::size_t kGroupWireSize = 64u;
inline constexpr std::size_t kGroupNameSize = 48u;
inline constexpr std::size_t kMechCount = 64u;

/** FourCC-тег секции групп в шоуфайле — "GRUP". */
inline constexpr uint32_t kGroupSectionTag = 0x50555247u;

/** Выбор штанкетов 0..63 — бит N означает «механизм N включён». */
class Selection {
    uint64_t bits_ = 0;

    constexpr explicit Selection(uint64_t raw) noexcept : bits_(raw) {}

public:
    constexpr Selection() noexcept = default;

    static constexpr Selection from_raw(uint64_t raw) noexcept { return Selection(raw); }

    [[nodiscard]] constexpr uint64_t raw() const noexcept { return bits_; }

    [[nodiscard]] constexpr bool empty() const noexcept { return bits_ == 0ULL; }

    [[nodiscard]] constexpr bool any() const noexcept { return !empty(); }

    [[nodiscard]] constexpr bool contains(uint8_t id) const noexcept
    {
        return id < kMechCount && ((bits_ >> id) & 1ULL) != 0ULL;
    }

    constexpr void add(uint8_t id) noexcept
    {
        if (id < kMechCount) {
            bits_ |= (1ULL << id);
        }
    }

    constexpr void remove(uint8_t id) noexcept
    {
        if (id < kMechCount) {
            bits_ &= ~(1ULL << id);
        }
    }

    constexpr Selection operator|(Selection o) const noexcept { return Selection(bits_ | o.bits_); }

    constexpr Selection operator&(Selection o) const noexcept { return Selection(bits_ & o.bits_); }

    constexpr Selection operator~() const noexcept { return Selection(~bits_); }
};

static_assert(sizeof(Selection) == sizeof(uint64_t));

#pragma pack(push, 1)

/** Группа механизмов — 64 байта. */
struct Group {
    enum class Flag : uint8_t {
        Blocked = 1u << 0,
        Atomic  = 1u << 1
    };

    uint8_t id = 0;
    char name[kGroupNameSize]{};
    Selection mech;
    REG::BitMask<Flag> flag;
    uint8_t reserved[4]{};
    uint16_t crc16 = 0;
};

static_assert(sizeof(Group) == kGroupWireSize);

#pragma pack(pop)

REG_BITMASK_ENUM_OPS(Group::Flag)

} // namespace smcp
