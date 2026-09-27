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
inline constexpr uint8_t kMechCount = 32u;
/** Пользовательских групп в шоуфайле (слоты UI 0…31). */
inline constexpr uint8_t kGroupMaxCount = 32u;

/** FourCC-тег секции групп в шоуфайле — "GRUP". */
inline constexpr uint32_t kGroupSectionTag = 0x50555247u;

/** Выбор штанкетов 0..31 — бит N означает «механизм N включён» (один server ID). */
class Selection {
public:
    constexpr Selection() noexcept = default;

    /** Собрать маску из сырого uint32. */
    [[nodiscard]] static constexpr Selection from_raw(uint32_t raw) noexcept
    {
        return Selection(raw);
    }

    [[nodiscard]] constexpr uint32_t raw() const noexcept { return bits_; }
    [[nodiscard]] constexpr bool empty() const noexcept { return bits_ == 0u; }
    [[nodiscard]] constexpr bool any() const noexcept { return !empty(); }

    /** Ось @a id входит в маску. */
    [[nodiscard]] constexpr bool contains(uint8_t id) const noexcept
    {
        return id < kMechCount && ((bits_ >> id) & 1u) != 0u;
    }

    /** Число установленных бит. */
    [[nodiscard]] constexpr uint8_t count() const noexcept
    {
        uint8_t n = 0;
        for (uint32_t v = bits_; v != 0u; v >>= 1) {
            n = static_cast<uint8_t>(n + (v & 1u));
        }
        return n;
    }

    /** Включить бит оси; id вне 0..31 — no-op. */
    constexpr void add(uint8_t id) noexcept
    {
        if (id < kMechCount) {
            bits_ |= (1u << id);
        }
    }
    /** Снять бит оси; id вне 0..31 — no-op. */
    constexpr void remove(uint8_t id) noexcept
    {
        if (id < kMechCount) {
            bits_ &= ~(1u << id);
        }
    }
    /** Инвертировать бит оси; id вне 0..31 — no-op. */
    constexpr void toggle(uint8_t id) noexcept
    {
        if (id < kMechCount) {
            bits_ ^= (1u << id);
        }
    }

    constexpr Selection operator|(Selection o) const noexcept { return Selection(bits_ | o.bits_); }
    constexpr Selection operator&(Selection o) const noexcept { return Selection(bits_ & o.bits_); }
    constexpr Selection operator~() const noexcept { return Selection(~bits_); }

private:
    constexpr explicit Selection(uint32_t raw) noexcept : bits_(raw) {}

    uint32_t bits_ = 0;
};

static_assert(sizeof(Selection) == sizeof(uint32_t));

/**
 * Группа механизмов — 64 байта, запись секции GRUP (только wire).
 *
 * Layout:
 *   0 id(1) | 1 flag(1) | 2 reserved_1(2) | 4 mech(4) | 8 name(48) | 56 reserved_2(8)
 *
 * reserved_1 после flag: 2 байта до выравнивания uint32 у mech.
 */
struct Group {
    enum class Flag : uint8_t {
        Blocked = 1u << 0,
        Atomic = 1u << 1,
    };

    uint8_t id = 0;
    REG::BitMask<Flag> flag;
    uint8_t reserved_1[2]{};
    Selection mech;
    char name[kGroupNameSize]{};
    uint8_t reserved_2[8]{};

    /** Нет выбранных осей. */
    [[nodiscard]] constexpr bool isEmpty() const noexcept { return mech.empty(); }
    /** Флаг Blocked в записи GRUP (не сегментный Block на шине). */
    [[nodiscard]] constexpr bool isBlocked() const noexcept { return flag.any(Flag::Blocked); }

    /** Сбросить payload; id слота не трогаем. */
    void clear() noexcept;
    /** Скопировать имя с нуль-терминатором. */
    void setName(const char* group_name) noexcept;
    /** @a on — целевое состояние, не toggle. */
    void setBlocked(bool on = true) noexcept;
};

static_assert(sizeof(Group) == kGroupWireSize);
static_assert(alignof(Group) == alignof(uint32_t));
static_assert(offsetof(Group, id) == 0);
static_assert(offsetof(Group, flag) == 1);
static_assert(offsetof(Group, reserved_1) == 2);
static_assert(offsetof(Group, mech) == 4);
static_assert(offsetof(Group, name) == 8);
static_assert(offsetof(Group, reserved_2) == 56);

REG_BITMASK_ENUM_OPS(Group::Flag)

} // namespace smcp
