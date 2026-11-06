/**
 * @file group_bank.hpp
 * @brief IGroupBank, CGroupBank, CGMech — секция GRUP и оси пульта.
 */

#pragma once

#include <cstring>

#include "showFile.hpp"
#include "segment/console.hpp"
#include "smcp/show/group.hpp"
#include "smcp/show/group_console.hpp"
#include "smcp/show/console.hpp"

namespace smcp::show {

/**
 * Банк групп без N: пересечения с blocked и markEdited.
 * CGroupBank<N> реализует проходку по своим слотам.
 */
class IGroupBank {
public:
    static constexpr uint8_t kNoExcept = 0xFFu;

    /** Одна blocked-группа и оси, пересекшиеся с последней операцией. */
    struct OverlapSlot {
        uint8_t group = 0xFFu;
        segment::Selection sel{};
    };

    /** Буфер слотов вызывающего + сколько записали. */
    struct Overlap {
        OverlapSlot* slot = nullptr;
        uint8_t capacity = 0;
        uint8_t count = 0;

        [[nodiscard]] segment::Selection sels() const noexcept
        {
            segment::Selection all;
            if (slot == nullptr) {
                return all;
            }
            for (uint8_t i = 0; i < count; ++i) {
                all = all | slot[i].sel;
            }
            return all;
        }
    };

    virtual ~IGroupBank() = default;

    /** Буфер слотов; владеет вызывающий. nullptr / 0 — только факт пересечения. */
    void setOverlapArray(OverlapSlot* slot, uint8_t capacity) noexcept
    {
        _overlap.slot = slot;
        _overlap.capacity = (slot != nullptr) ? capacity : 0u;
        _overlap.count = 0;
    }

    virtual void markEdited() noexcept = 0;
    [[nodiscard]] const Overlap& overlap() const noexcept { return _overlap; }

    [[nodiscard]] virtual bool containsBlocked(uint8_t axis_id) const noexcept = 0;

    /**
     * Пересечение @a mask с blocked, кроме @a except_id (kNoExcept — все).
     * Пишет overlap(); true — есть общие оси.
     */
    virtual bool fillBlockedOverlap(uint8_t except_id, segment::Selection mask) noexcept = 0;

protected:
    Overlap _overlap{};
};

/**
 * Секция GRUP + банк слотов: payload — Group[N], operator[] даёт временный CGroup.
 */
template <uint8_t N>
class CGroupBank : public sf::Section<Group, N>, public IGroupBank {
    using Base = sf::Section<Group, N>;

public:
    static constexpr uint8_t kCount = N;

    /** Id слотов = индекс 0…N-1. */
    CGroupBank(sf::IShow& file, GroupConsole& console, bool required = true) noexcept
        : Base(file, kGroupSectionTag, required)
        , _console(console)
    {
        for (uint8_t i = 0; i < N; ++i) {
            this->rec(i).id = i;
        }
    }

    /** Сбросить все слоты, сохранив id = индекс. */
    void clearData() noexcept override
    {
        for (uint8_t i = 0; i < N; ++i) {
            this->rec(i).clear();
            this->rec(i).id = i;
        }
        this->clearDesc();
        _overlap.count = 0;
    }

    /** Известные флаги и нуль-терминатор в имени. */
    [[nodiscard]] bool isValid() const noexcept override
    {
        constexpr uint8_t kKnown = static_cast<uint8_t>(Group::Flag::Blocked)
            | static_cast<uint8_t>(Group::Flag::Atomic);
        for (uint8_t i = 0; i < N; ++i) {
            const Group& g = this->rec(i);
            if ((g.flag.raw() & static_cast<uint8_t>(~kKnown)) != 0u) {
                return false;
            }
            if (std::memchr(g.name, '\0', kGroupNameSize) == nullptr) {
                return false;
            }
        }
        return true;
    }

    /** Временный прокси слота (не хранить). */
    [[nodiscard]] CGroup operator[](uint8_t i) noexcept
    {
        return CGroup(this->rec(i < N ? i : 0u), *this, _console);
    }
    [[nodiscard]] CGroup operator[](uint8_t i) const noexcept
    {
        return const_cast<CGroupBank*>(this)->operator[](i);
    }

    void markEdited() noexcept override { Base::markEdited(); }

    [[nodiscard]] bool containsBlocked(uint8_t axis_id) const noexcept override
    {
        for (uint8_t i = 0; i < N; ++i) {
            const Group& g = this->rec(i);
            if (g.isBlocked() && g.sel.contains(axis_id)) {
                return true;
            }
        }
        return false;
    }

    bool fillBlockedOverlap(uint8_t except_id, segment::Selection mask) noexcept override
    {
        _overlap.count = 0;
        bool hit = false;
        for (uint8_t i = 0; i < N; ++i) {
            const Group& g = this->rec(i);
            if (g.id == except_id || !g.isBlocked()) {
                continue;
            }
            const segment::Selection common = g.sel & mask;
            if (common.empty()) {
                continue;
            }
            hit = true;
            if (_overlap.slot != nullptr && _overlap.count < _overlap.capacity) {
                _overlap.slot[_overlap.count].group = g.id;
                _overlap.slot[_overlap.count].sel = common;
                ++_overlap.count;
            }
        }
        return hit;
    }

private:
    GroupConsole& _console;
};

/** show::Axis + банк групп: отказ Select при сегментном Block или пересечении GRUP. */
class CGMech : public Axis {
public:
    CGMech(segment::Console& console, IGroupBank& groups) noexcept;

    /** Deselect — Ok. Add — Blocked / OverlapsBlocked / Occupied / NotSent (TX через Patch). */
    [[nodiscard]] CGroup::Result trySelect(uint8_t console_id) noexcept;

private:
    IGroupBank& _groups;
};

template <uint8_t N>
using CGMechBank = segment::console::AxisStore<N, CGMech>;

} // namespace smcp::show
