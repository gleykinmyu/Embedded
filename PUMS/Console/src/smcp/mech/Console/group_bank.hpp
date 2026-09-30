/**
 * @file group_bank.hpp
 * @brief IGroupBank, CGroup, CGroupBank, CGMech — секция GRUP и оси пульта.
 */

#pragma once

#include "obj_bank.hpp"
#include "showFile.hpp"
#include "smcp/mech/Console/cmech.hpp"
#include "smcp/mech/group.hpp"
#include "smcp/mech/Console/group_console.hpp"

namespace smcp {

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
        Selection mech{};
    };

    /** Буфер слотов вызывающего + сколько записали. */
    struct Overlap {
        OverlapSlot* slot = nullptr;
        uint8_t capacity = 0;
        uint8_t count = 0;

        [[nodiscard]] Selection mechs() const noexcept
        {
            Selection all;
            if (slot == nullptr) {
                return all;
            }
            for (uint8_t i = 0; i < count; ++i) {
                all = all | slot[i].mech;
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
    /** Ось входит в blocked-группу. */
    [[nodiscard]] virtual bool containsBlocked(uint8_t mech_id) const noexcept = 0;
    /**
     * Пересечение @a mask маски @a index с blocked, кроме @a except_id (kNoExcept — все).
     * @a index — номер с 0. Пишет overlap(); true — есть общие оси.
     */
    virtual bool fillBlockedOverlap(uint8_t except_id, uint8_t index, Selection mask) noexcept = 0;

protected:
    Overlap _overlap{};
};

/**
 * Секция GRUP: payload — Group<Segments>[N]. operator[] даёт временный CGroup.
 * Segments — сколько масок в слоте (серверов). По умолчанию 1, слот 64 байта.
 */
template <uint8_t N, uint8_t Segments = 1u>
class CGroupBank : public sf::Section<Group<Segments>, N>, public IGroupBank {
    using Base = sf::Section<Group<Segments>, N>;

public:
    static constexpr uint8_t kCount = N;
    static constexpr uint8_t kSegments = Segments;

    /** Id слотов = индекс 0…N-1. */
    CGroupBank(sf::IShow& file, IGroupConsole& console, bool required = true) noexcept
        : Base(file, kGroupSectionTag, required)
        , _console(console)
    {
        for (uint8_t i = 0; i < N; ++i) {
            this->rec(i).head.id = i;
            this->rec(i).head.count = Segments;
        }
    }

    /** Сбросить заголовки слотов. id и count не трогаем. */
    void clearData() noexcept override
    {
        for (uint8_t i = 0; i < N; ++i) {
            this->rec(i).head.clear();
        }
        this->clearDesc();
        _overlap.count = 0;
    }

    /** Известные флаги и нуль-терминатор в имени. */
    [[nodiscard]] bool isValid() const noexcept override
    {
        constexpr uint8_t kKnown = static_cast<uint8_t>(GroupHeader::Flag::Blocked)
            | static_cast<uint8_t>(GroupHeader::Flag::Atomic);
        for (uint8_t i = 0; i < N; ++i) {
            const Group<Segments>& g = this->rec(i);
            if (g.head.count != Segments) {
                return false;
            }
            if ((g.head.flag.raw() & static_cast<uint8_t>(~kKnown)) != 0u) {
                return false;
            }
            if (std::memchr(g.head.name, '\0', kGroupNameSize) == nullptr) {
                return false;
            }
        }
        return true;
    }

    /** Временный прокси слота (не хранить). */
    [[nodiscard]] CGroup operator[](uint8_t i) noexcept
    {
        return CGroup(this->rec(i), *this, _console);
    }
    [[nodiscard]] const CGroup operator[](uint8_t i) const noexcept
    {
        return CGroup(const_cast<Group<Segments>&>(this->rec(i)),
                      const_cast<CGroupBank&>(*this),
                      const_cast<IGroupConsole&>(_console));
    }

    void markEdited() noexcept override { Base::markEdited(); }

    [[nodiscard]] bool containsBlocked(uint8_t mech_id) const noexcept override
    {
        for (uint8_t i = 0; i < N; ++i) {
            const Group<Segments>& g = this->rec(i);
            const CGroup group(const_cast<GroupHeader*>(&g.head), const_cast<Selection*>(g.sel));
            if (!g.head.isBlocked()) {
                continue;
            }
            for (uint8_t seg = 0u; seg < g.head.count; ++seg) {
                if (group.contains(seg, mech_id)) {
                    return true;
                }
            }
        }
        return false;
    }

    bool fillBlockedOverlap(uint8_t except_id, uint8_t index, Selection mask) noexcept override
    {
        _overlap.count = 0;
        bool any = false;
        for (uint8_t i = 0; i < N; ++i) {
            const Group<Segments>& g = this->rec(i);
            if (g.head.id == except_id || !g.head.isBlocked()) {
                continue;
            }
            const CGroup group(const_cast<GroupHeader*>(&g.head), const_cast<Selection*>(g.sel));
            const Selection* const sel = group.at(index);
            if (sel == nullptr) {
                continue;
            }
            const Selection hit = *sel & mask;
            if (!hit.any()) {
                continue;
            }
            any = true;
            if (_overlap.slot != nullptr && _overlap.count < _overlap.capacity) {
                OverlapSlot& dst = _overlap.slot[_overlap.count++];
                dst.group = g.head.id;
                dst.mech = hit;
            }
        }
        return any;
    }

private:
    using Base::begin;
    using Base::end;

    IGroupConsole& _console;
};

/** CMech + банк групп: отказ Select при сегментном Block или пересечении GRUP. */
class CGMech : public CMech {
public:
    CGMech(IConsole& console, IGroupBank& groups, uint8_t id) noexcept;
    CGMech(IConsole& console, IGroupBank& groups, uint8_t server_id, uint8_t id) noexcept;

    /** Deselect — Ok, если Select в очереди. Add — Blocked / OverlapsBlocked / Occupied / NotSent. */
    [[nodiscard]] CGroup::Result trySelect(uint8_t console_id) noexcept;

private:
    IGroupBank& _groups;
};

template <uint8_t N>
using CGMechBank = MISC::ObjBank<CGMech, N>;

} // namespace smcp
