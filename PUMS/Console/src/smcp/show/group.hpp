/**
 * @file group.hpp
 * @brief Структуры данных групп штанкетов SMCP v1.5 (блок 3.3).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bitmask.hpp"
#include "segment/selection.hpp"
#include "tags.hpp"

namespace smcp {

/** Алиас: segment::kMechCount. */
inline constexpr uint8_t kMechCount = segment::kMechCount;

namespace show {

inline constexpr std::size_t kGroupWireSize = 64u;
inline constexpr std::size_t kGroupNameSize = 48u;
/** Пользовательских групп в шоуфайле (слоты UI 0…31). */
inline constexpr uint8_t kGroupMaxCount = 32u;

/** FourCC-тег секции групп в шоуфайле — "GRUP". */
inline constexpr sf::Tag kGroupSectionTag = 0x50555247u;

/**
 * Группа механизмов — 64 байта, запись секции GRUP (только wire).
 *
 * Layout:
 *   0 id(1) | 1 flag(1) | 2 reserved_1(2) | 4 sel(4) | 8 name(48) | 56 reserved_2(8)
 *
 * reserved_1 после flag: 2 байта до выравнивания uint32 у sel.
 */
struct Group {
    enum class Flag : uint8_t {
        Blocked = 1u << 0,
        Atomic = 1u << 1,
        All = Blocked | Atomic,
    };

    uint8_t id = 0;
    REG::BitMask<Flag> flag;
    uint8_t reserved_1[2]{};
    segment::Selection sel;
    char name[kGroupNameSize]{};
    uint8_t reserved_2[8]{};

    /** Нет выбранных осей. */
    [[nodiscard]] constexpr bool isEmpty() const noexcept { return sel.empty(); }
    /** Флаг Blocked в записи GRUP (не сегментный Block на шине). */
    [[nodiscard]] constexpr bool isBlocked() const noexcept { return flag.any(Flag::Blocked); }

    /** Сбросить payload; id слота не трогаем. */
    void clear() noexcept
    {
        sel = segment::Selection{};
        flag = {};
        std::memset(reserved_1, 0, sizeof(reserved_1));
        std::memset(name, 0, sizeof(name));
        std::memset(reserved_2, 0, sizeof(reserved_2));
    }
    /** Скопировать имя с нуль-терминатором. */
    void setName(const char* group_name) noexcept
    {
        if (group_name == nullptr) {
            name[0] = '\0';
            return;
        }
        std::strncpy(name, group_name, kGroupNameSize - 1u);
        name[kGroupNameSize - 1u] = '\0';
    }
    /** @a on — целевое состояние, не toggle. */
    void setBlocked(bool on = true) noexcept
    {
        if (on) {
            flag.set(Flag::Blocked);
        } else {
            flag.clear(Flag::Blocked);
        }
    }
};

static_assert(sizeof(Group) == kGroupWireSize);
static_assert(alignof(Group) == alignof(uint32_t));
static_assert(offsetof(Group, id) == 0);
static_assert(offsetof(Group, flag) == 1);
static_assert(offsetof(Group, reserved_1) == 2);
static_assert(offsetof(Group, sel) == 4);
static_assert(offsetof(Group, name) == 8);
static_assert(offsetof(Group, reserved_2) == 56);

REG_BITMASK_ENUM_OPS(Group::Flag)

} // namespace show

} // namespace smcp
