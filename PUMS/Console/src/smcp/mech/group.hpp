/**
 * @file group.hpp
 * @brief Структуры данных групп штанкетов SMCP v1.5 (блок 3.3).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "bitmask.hpp"

namespace smcp {

inline constexpr std::size_t kGroupNameSize = 48u;
/** Максимум осей на один server ID (бит Selection / GRUP). 64 оси → два SMCP ID. Inventory — MaxMechs у Server/Console. */
inline constexpr uint8_t kMechCount = 32u;
/** Пользовательских групп в шоуфайле (слоты UI 0…31). */
inline constexpr uint8_t kGroupMaxCount = 32u;
/** Сколько масок Selection умещает один слот GRUP. */
inline constexpr uint8_t kMaxGroupSegments = 4u;

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
    [[nodiscard]] constexpr operator uint32_t() const noexcept { return bits_; }
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

    /** Маска из одной оси. */
    [[nodiscard]] static constexpr Selection of(uint8_t id) noexcept
    {
        Selection s{};
        s.add(id);
        return s;
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
 * Заголовок слота GRUP. Маски Selection[Segments] лежат сразу за ним в Group<Segments>.
 * count — сколько серверов заявлено: маска есть на каждый.
 */
struct GroupHeader {
    enum class Flag : uint8_t {
        Blocked = 1u << 0,
        Atomic = 1u << 1,
    };

    uint8_t id = 0;
    REG::BitMask<Flag> flag;
    uint8_t count = 0;
    uint8_t reserved = 0;
    char name[kGroupNameSize]{};
    uint8_t reserved_2[8]{};

    [[nodiscard]] constexpr bool isBlocked() const noexcept { return flag.any(Flag::Blocked); }

    /** @a on — целевое состояние, не toggle. */
    void setBlocked(bool on = true) noexcept
    {
        if (on) {
            flag.set(Flag::Blocked);
        } else {
            flag.clear(Flag::Blocked);
        }
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

    /** Сбросить заголовок. id и count не входят в сброс. */
    void clear() noexcept
    {
        flag = {};
        reserved = 0u;
        std::memset(name, 0, sizeof(name));
        std::memset(reserved_2, 0, sizeof(reserved_2));
    }
};

/**
 * Слот GRUP: заголовок и Selection[Segments].
 * Сегмент — номер с 0. Id сервера по индексу даёт IConsole::serverId.
 */
template <uint8_t Segments>
struct Group {
    static_assert(Segments >= 1u && Segments <= kMaxGroupSegments, "Group segments");

    static constexpr uint8_t kSegments = Segments;

    GroupHeader head;
    Selection sel[Segments]{};
};

static_assert(std::is_standard_layout<Group<1>>::value);
static_assert(offsetof(Group<1>, head) == 0);
static_assert(offsetof(Group<1>, sel) == sizeof(GroupHeader));
static_assert(alignof(Group<1>) == alignof(uint32_t));

REG_BITMASK_ENUM_OPS(GroupHeader::Flag)

} // namespace smcp
