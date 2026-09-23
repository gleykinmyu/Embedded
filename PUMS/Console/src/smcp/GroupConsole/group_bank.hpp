/**
 * @file group_bank.hpp
 * @brief IGroupBank, CGroup, CGroupBank, CGMech — секция GRUP и оси пульта.
 */

#pragma once

#include "obj_bank.hpp"
#include "showFile.hpp"
#include "smcp/Console/cmech.hpp"
#include "smcp/GroupConsole/group.hpp"
#include "smcp/GroupConsole/group_console.hpp"

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
     * Пересечение @a mask с blocked, кроме @a except_id (kNoExcept — все).
     * Пишет overlap(); true — есть общие оси.
     */
    virtual bool fillBlockedOverlap(uint8_t except_id, Selection mask) noexcept = 0;

protected:
    Overlap _overlap{};
};

/**
 * Прокси слота GRUP: запись в секции + пульт. В шоуфайл пишется Group, не этот объект.
 * Временный: не хранить, только сразу вызвать метод.
 */
class CGroup {
public:
    /** Прокси не владеет записью: @a rec живёт в секции. */
    CGroup(Group& rec, IGroupBank& bank, IGroupConsole& console) noexcept;

    [[nodiscard]] uint8_t id() const noexcept;
    /** Маска осей слота. */
    [[nodiscard]] const Selection& mech() const noexcept;
    [[nodiscard]] REG::BitMask<Group::Flag> flags() const noexcept;
    [[nodiscard]] const char* name() const noexcept;

    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] bool isBlocked() const noexcept;
    /** Ставит Blocked. При @a on: overlap (инфо); если это queued/active — сброс группы. */
    void setBlocked(bool on = true) noexcept;

    enum class Result : uint8_t {
        Ok = 0,
        Empty,           /**< Пустая маска / слот. */
        Occupied,        /**< Слот непустой, нужен confirmed. */
        Blocked,         /**< Сама группа blocked. */
        OverlapsBlocked, /**< Пересечение с другой заблокированной группой. */
    };

    /**
     * Select маски и билет queued group.
     * Empty / Blocked / OverlapsBlocked — отказ.
     */
    [[nodiscard]] Result recall() noexcept;

    /**
     * Записать выделение в слот.
     * Occupied снимается @a confirmed; OverlapsBlocked — нет.
     */
    [[nodiscard]] Result record(Selection selection, const char* name = nullptr,
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
    void writeTo(Group& rec) const noexcept;

private:
    Group& _rec;
    IGroupBank& _bank;
    IGroupConsole& _console;
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
    CGroupBank(sf::IShow& file, IGroupConsole& console, bool required = true) noexcept
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
        return CGroup(this->rec(i), *this, _console);
    }
    [[nodiscard]] const CGroup operator[](uint8_t i) const noexcept
    {
        return CGroup(const_cast<Group&>(this->rec(i)), const_cast<CGroupBank&>(*this),
                      const_cast<IGroupConsole&>(_console));
    }

    void markEdited() noexcept override { Base::markEdited(); }

    [[nodiscard]] bool containsBlocked(uint8_t mech_id) const noexcept override
    {
        for (uint8_t i = 0; i < N; ++i) {
            const Group& g = this->rec(i);
            if (g.isBlocked() && g.mech.contains(mech_id)) {
                return true;
            }
        }
        return false;
    }

    bool fillBlockedOverlap(uint8_t except_id, Selection mask) noexcept override
    {
        _overlap.count = 0;
        bool any = false;
        for (uint8_t i = 0; i < N; ++i) {
            const Group& g = this->rec(i);
            if (g.id == except_id || !g.isBlocked()) {
                continue;
            }
            const Selection hit = g.mech & mask;
            if (!hit.any()) {
                continue;
            }
            any = true;
            if (_overlap.slot != nullptr && _overlap.count < _overlap.capacity) {
                OverlapSlot& dst = _overlap.slot[_overlap.count++];
                dst.group = g.id;
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

    /** Deselect — Ok + TX; Add — Blocked / OverlapsBlocked / Occupied, иначе TX. */
    [[nodiscard]] CGroup::Result trySelect(uint8_t console_id) noexcept;

private:
    IGroupBank& _groups;
};

template <uint8_t N>
using CGMechBank = MISC::ObjBank<CGMech, N>;

} // namespace smcp
