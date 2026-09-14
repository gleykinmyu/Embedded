/**
 * @file group.hpp
 * @brief Структуры данных групп штанкетов SMCP v1.5 (блок 3.3).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bitmask.hpp"
#include "smcp/Console/show_model.hpp"

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
        Atomic  = 1u << 1
    };
    uint8_t id = 0;
    REG::BitMask<Flag> flag;
    uint8_t reserved_1[2]{};
    Selection mech;
    char name[kGroupNameSize]{};
    uint8_t reserved_2[8]{};

    [[nodiscard]] constexpr bool isEmpty() const noexcept { return mech.empty(); }

    void clear() noexcept
    {
        mech = Selection{};
        flag = {};
        std::memset(reserved_1, 0, sizeof(reserved_1));
        std::memset(name, 0, sizeof(name));
        std::memset(reserved_2, 0, sizeof(reserved_2));
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
};

static_assert(sizeof(Group) == kGroupWireSize);
static_assert(alignof(Group) == alignof(uint32_t));
static_assert(offsetof(Group, id) == 0);
static_assert(offsetof(Group, flag) == 1);
static_assert(offsetof(Group, reserved_1) == 2);
static_assert(offsetof(Group, mech) == 4);
static_assert(offsetof(Group, name) == 8);
static_assert(offsetof(Group, reserved_2) == 56);


class IConsole;

/**
 * Прокси слота GRUP: ссылки на запись в секции и на пульт.
 * В шоуфайл пишется Group (64 байта), не этот объект.
 */
class CGroup {
    IConsole& _console;
    Group& _rec;
    file::ISection& _section;

public:
    CGroup(IConsole& console, Group& rec, file::ISection& section) noexcept
        : _console(console)
        , _rec(rec)
        , _section(section)
    {}

    [[nodiscard]] uint8_t id() const noexcept { return _rec.id; }
    [[nodiscard]] const Selection& mech() const noexcept { return _rec.mech; }
    [[nodiscard]] REG::BitMask<Group::Flag> flags() const noexcept { return _rec.flag; }
    [[nodiscard]] const char* name() const noexcept { return _rec.name; }

    [[nodiscard]] bool isEmpty() const noexcept { return _rec.isEmpty(); }
    [[nodiscard]] bool isBlocked() const noexcept { return _rec.isBlocked(); }
    void setBlocked(bool on = true) noexcept;

    /** Select маски группы. */
    [[nodiscard]] bool recall() noexcept;

    /**
     * Записать выделение в слот.
     * Пустая — всегда; непустая — только @a confirmed (перезапись).
     */
    [[nodiscard]] bool record(Selection selection,
                              const char* name = nullptr,
                              bool confirmed = false) noexcept;
    /** Переименовать непустую группу. */
    [[nodiscard]] bool rename(const char* name) noexcept;
    /**
     * Очистить слот.
     * Пустая — true; непустая — только @a confirmed.
     */
    [[nodiscard]] bool clear(bool confirmed = false) noexcept;

    /** Прочитать запись шоуфайла; id слота не меняется. */
    void readFrom(const Group& rec) noexcept;
    /** Выгрузить запись шоуфайла. */
    void writeTo(Group& rec) const noexcept { rec = _rec; }
};

/**
 * Секция GRUP + банк слотов: payload — Group[N], operator[] даёт временный CGroup.
 */
template <uint8_t N>
class CGroupBank : public file::Section<Group, N> {
    using Base = file::Section<Group, N>;
    IConsole& _console;

    using Base::begin;
    using Base::end;

public:
    static constexpr uint8_t kCount = N;

    CGroupBank(file::IShowFile& file, IConsole& console, bool required = true) noexcept
        : Base(file, kGroupSectionTag, required)
        , _console(console)
    {
        for (uint8_t i = 0; i < N; ++i) {
            this->rec(i).id = i;
        }
    }

    [[nodiscard]] CGroup operator[](uint8_t i) noexcept
    {
        return CGroup(_console, this->rec(i), *this);
    }

    [[nodiscard]] const CGroup operator[](uint8_t i) const noexcept
    {
        return CGroup(_console, const_cast<Group&>(this->rec(i)),
                      const_cast<CGroupBank&>(*this));
    }
};


REG_BITMASK_ENUM_OPS(Group::Flag)

} // namespace smcp
