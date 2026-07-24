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
/** Осей на один server ID. Физический сервер на 64 оси → два SMCP ID. */
inline constexpr std::size_t kMechCount = 32u;
inline constexpr std::size_t kGroupMaxCount = 33u;

/** FourCC-тег секции групп в шоуфайле — "GRUP". */
inline constexpr uint32_t kGroupSectionTag = 0x50555247u;

/** Выбор штанкетов 0..31 — бит N означает «механизм N включён» (один server ID). */
class Selection {
    uint32_t bits_ = 0;

    constexpr explicit Selection(uint32_t raw) noexcept : bits_(raw) {}

public:
    constexpr Selection() noexcept = default;

    static constexpr Selection from_raw(uint32_t raw) noexcept { return Selection(raw); }

    [[nodiscard]] constexpr uint32_t raw() const noexcept { return bits_; }

    [[nodiscard]] constexpr bool empty() const noexcept { return bits_ == 0u; }

    [[nodiscard]] constexpr bool any() const noexcept { return !empty(); }

    [[nodiscard]] constexpr bool contains(uint8_t id) const noexcept
    {
        return id < kMechCount && ((bits_ >> id) & 1u) != 0u;
    }

    constexpr void add(uint8_t id) noexcept
    {
        if (id < kMechCount) {
            bits_ |= (1u << id);
        }
    }

    constexpr void remove(uint8_t id) noexcept
    {
        if (id < kMechCount) {
            bits_ &= ~(1u << id);
        }
    }

    constexpr void toggle(uint8_t id) noexcept
    {
        if (id < kMechCount) {
            bits_ ^= (1u << id);
        }
    }

    [[nodiscard]] constexpr uint8_t count() const noexcept
    {
        uint8_t n = 0;
        for (uint32_t v = bits_; v != 0u; v >>= 1) {
            n = static_cast<uint8_t>(n + (v & 1u));
        }
        return n;
    }

    constexpr Selection operator|(Selection o) const noexcept { return Selection(bits_ | o.bits_); }

    constexpr Selection operator&(Selection o) const noexcept { return Selection(bits_ & o.bits_); }

    constexpr Selection operator~() const noexcept { return Selection(~bits_); }
};

static_assert(sizeof(Selection) == sizeof(uint32_t));


/**
 * Группа механизмов — 64 байта (wire = runtime).
 *
 * Layout:
 *   0 id(1) | 1 flag(1) | 2 crc16(2) | 4 reserved(4) | 8 mech(4) | 12 pad(4) | 16 name(48)
 */
struct Group {
    enum class Flag : uint8_t {
        Blocked = 1u << 0,
        Atomic  = 1u << 1
    };
    uint8_t id = 0;
    REG::BitMask<Flag> flag;
    uint16_t crc16 = 0;
    uint8_t reserved[4]{};
    Selection mech;
    uint8_t pad[4]{};
    char name[kGroupNameSize]{};

    [[nodiscard]] constexpr bool isEmpty() const noexcept { return mech.empty(); }

    void clear() noexcept
    {
        mech = Selection{};
        flag = {};
        crc16 = 0;
        std::memset(reserved, 0, sizeof(reserved));
        std::memset(pad, 0, sizeof(pad));
        std::memset(name, 0, sizeof(name));
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

static_assert(sizeof(Group) == kGroupWireSize);
static_assert(alignof(Group) == alignof(uint32_t));


REG_BITMASK_ENUM_OPS(Group::Flag)

} // namespace smcp
