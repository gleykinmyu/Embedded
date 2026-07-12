/**
 * @file group.hpp
 * @brief Структуры данных групп штанкетов SMCP v1.5 (блок 3.3).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bitmask.hpp"

namespace smcp {

inline constexpr std::size_t kGroupWireSize = 64u;
inline constexpr std::size_t kGroupNameSize = 48u;
inline constexpr std::size_t kMechCount = 64u;
inline constexpr std::size_t kGroupMaxCount = 33u;

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

    constexpr void toggle(uint8_t id) noexcept
    {
        if (id < kMechCount) {
            bits_ ^= (1ULL << id);
        }
    }

    [[nodiscard]] constexpr std::size_t count() const noexcept
    {
        std::size_t n = 0;
        for (uint64_t v = bits_; v != 0ULL; v >>= 1) {
            n += static_cast<std::size_t>(v & 1ULL);
        }
        return n;
    }

    constexpr Selection operator|(Selection o) const noexcept { return Selection(bits_ | o.bits_); }

    constexpr Selection operator&(Selection o) const noexcept { return Selection(bits_ & o.bits_); }

    constexpr Selection operator~() const noexcept { return Selection(~bits_); }
};

static_assert(sizeof(Selection) == sizeof(uint64_t));


/** Группа механизмов — 64 байта. */
struct Group {
    enum class Flag : uint8_t {
        Blocked = 1u << 0,
        Atomic  = 1u << 1
    };
    uint8_t id = 0;
    REG::BitMask<Flag> flag;
    Selection mech;
    char name[kGroupNameSize]{};
    uint8_t reserved[4]{};
    uint16_t crc16 = 0;

    [[nodiscard]] constexpr bool isEmpty() const noexcept { return mech.empty(); }

    void clear() noexcept
    {
        mech = Selection{};
        name[0] = '\0';
        flag = {};
        crc16 = 0;
    }

    void setName(const char* group_name) noexcept
    {
        if (group_name == nullptr) {
            name[0] = '\0';
            return;
        }
        std::strncpy(name, group_name, kGroupNameSize - 1u);
        name[kGroupNameSize - 1u] = '\0';
    }

    void setSelection(Selection selection) noexcept { mech = selection; }

    [[nodiscard]] constexpr bool isBlocked() const noexcept { return flag.any(Flag::Blocked); }

    void setBlocked(bool blocked) noexcept
    {
        if (blocked) {
            flag.set(Flag::Blocked);
        } else {
            flag.clear(Flag::Blocked);
        }
    }
};


//static_assert(sizeof(Group) == kGroupWireSize);


REG_BITMASK_ENUM_OPS(Group::Flag)

} // namespace smcp
