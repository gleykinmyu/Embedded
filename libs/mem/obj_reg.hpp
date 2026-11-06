/**
 * @file mem/obj_reg.hpp
 * @brief mem::ObjReg / ObjStore — реестр T* по IdType как SlotBase для Store.
 *
 * ObjReg : protected IdSlot&lt;T*, IdType, FirstId&gt; — нумерация id + регистрация.
 * Владение: Store&lt;T*, IdType, Cap, ObjReg&lt;…&gt;&gt;.
 * Внешний буфер: ObjReg(cells, n).
 *
 *   #include "mem/obj_reg.hpp"
 *
 *   mem::ObjStore&lt;Axis, uint8_t, 16&gt; store;
 *   store.registerAt(0, &axis);
 *
 *   Axis* cells[16]{};
 *   mem::ObjReg&lt;Axis&gt; reg(cells, 16);
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "mem/slot.hpp"

namespace mem {

/**
 * Реестр указателей T* по IdType. SlotBase для Store: ctor(T**, capacity).
 *
 * База — IdSlot (FirstId + atId). Снаружи — API регистрации, не сырой ISlot.
 */
template <typename T, typename IdType = uint8_t, IdType FirstId = IdType{}>
class ObjReg : protected IdSlot<T*, IdType, FirstId> {
    static_assert(!std::is_pointer_v<T>, "ObjReg: T is the pointee type, not T*");
    static_assert(std::is_integral_v<IdType> && std::is_unsigned_v<IdType>,
                  "ObjReg: IdType must be unsigned integral");

    using Slot = IdSlot<T*, IdType, FirstId>;

public:
    enum class Status : uint8_t {
        Ok,
        NullEntry,
        IdOutOfRange,
        SlotOccupied,
        NoFreeSlot,
        IdNotSequential,
        SwappedWithOccupiedSlot,
    };

    using value_type = T;
    using id_type = IdType;
    using size_type = IdType;

    static constexpr IdType kFirstId = FirstId;

    /** Вид на буфер ячеек T* (Store / внешний массив). */
    ObjReg(T** cells, IdType capacity) noexcept
        : Slot(cells, capacity)
    {}

    ~ObjReg() = default;

    ObjReg(const ObjReg&) = delete;
    ObjReg& operator=(const ObjReg&) = delete;
    ObjReg(ObjReg&&) = delete;
    ObjReg& operator=(ObjReg&&) = delete;

    [[nodiscard]] static const char* cstr(Status v) noexcept
    {
        switch (v) {
        case Status::Ok:
            return "Ok";
        case Status::NullEntry:
            return "NullEntry";
        case Status::IdOutOfRange:
            return "IdOutOfRange";
        case Status::SlotOccupied:
            return "SlotOccupied";
        case Status::NoFreeSlot:
            return "NoFreeSlot";
        case Status::IdNotSequential:
            return "IdNotSequential";
        case Status::SwappedWithOccupiedSlot:
            return "SwappedWithOccupiedSlot";
        default:
            return "?";
        }
    }

    using Slot::capacity;
    using Slot::endId;
    using Slot::firstId;
    using Slot::idInRange;

    [[nodiscard]] IdType registeredCount() const noexcept { return _registered; }

    [[nodiscard]] T* next(IdType& id) noexcept
    {
        IdType cur = FirstId;
        if (id >= FirstId) {
            cur = static_cast<IdType>(static_cast<std::size_t>(id) + 1u);
        }
        const IdType end = endId();
        for (; cur < end; ++cur) {
            if (T* const e = slotAt(cur)) {
                id = cur;
                return e;
            }
        }
        return nullptr;
    }

    [[nodiscard]] bool isEmpty(IdType id) const noexcept
    {
        if (!idInRange(id)) {
            return true;
        }
        return slotAt(id) == nullptr;
    }

    [[nodiscard]] bool contains(IdType id) const noexcept { return !isEmpty(id); }

    [[nodiscard]] IdType firstFreeId() const noexcept { return firstFreeId(FirstId); }

    [[nodiscard]] IdType firstFreeId(IdType fromId) const noexcept
    {
        const IdType id = firstFreeFrom(fromId);
        return id < endId() ? id : endId();
    }

    [[nodiscard]] T* get(IdType id) noexcept
    {
        if (!idInRange(id)) {
            return nullptr;
        }
        return slotAt(id);
    }

    [[nodiscard]] const T* get(IdType id) const noexcept
    {
        return const_cast<ObjReg*>(this)->get(id);
    }

    [[nodiscard]] Status registerAt(IdType id, T* entry) noexcept
    {
        if (entry == nullptr) {
            return Status::NullEntry;
        }
        if (!idInRange(id)) {
            return Status::IdOutOfRange;
        }

        T*& cell = slotAt(id);
        if (cell != nullptr) {
            if (cell == entry) {
                return Status::Ok;
            }
            return Status::SlotOccupied;
        }

        cell = entry;
        ++_registered;
        entry->set_id(id);
        return Status::Ok;
    }

    [[nodiscard]] Status registerAuto(T* entry, IdType& assignedId) noexcept
    {
        return registerAuto(entry, assignedId, FirstId);
    }

    [[nodiscard]] Status registerAuto(T* entry, IdType& assignedId, IdType fromId) noexcept
    {
        if (entry == nullptr) {
            return Status::NullEntry;
        }

        const IdType id = firstFreeFrom(fromId);
        if (id >= endId()) {
            return Status::NoFreeSlot;
        }

        slotAt(id) = entry;
        ++_registered;
        assignedId = id;
        entry->set_id(assignedId);
        return Status::Ok;
    }

    [[nodiscard]] Status registerSeq(T* entry, IdType id) noexcept
    {
        if (entry == nullptr) {
            return Status::NullEntry;
        }
        if (!idInRange(id)) {
            return Status::IdOutOfRange;
        }
        if (Slot::indexOf(id) != _registered) {
            return Status::IdNotSequential;
        }
        return registerAt(id, entry);
    }

    void unregister(IdType id) noexcept
    {
        if (!idInRange(id)) {
            return;
        }
        T*& cell = slotAt(id);
        if (cell != nullptr) {
            cell = nullptr;
            if (_registered != IdType{}) {
                --_registered;
            }
        }
    }

    void clear() noexcept
    {
        for (IdType id = FirstId; id < endId(); ++id) {
            slotAt(id) = nullptr;
        }
        _registered = IdType{};
    }

    [[nodiscard]] Status rebind(IdType oldId, IdType newId) noexcept
    {
        if (!idInRange(oldId) || !idInRange(newId)) {
            return Status::IdOutOfRange;
        }

        T* const entry = slotAt(oldId);
        if (entry == nullptr) {
            return Status::NullEntry;
        }
        if (oldId == newId) {
            return Status::Ok;
        }

        T*& other = slotAt(newId);
        if (other == nullptr) {
            slotAt(oldId) = nullptr;
            slotAt(newId) = entry;
            entry->set_id(newId);
            return Status::Ok;
        }
        if (other == entry) {
            return Status::Ok;
        }

        slotAt(oldId) = other;
        slotAt(newId) = entry;
        entry->set_id(newId);
        other->set_id(oldId);
        return Status::SwappedWithOccupiedSlot;
    }

protected:
    /** Ячейка T*&; только при idInRange. */
    [[nodiscard]] T*& slotAt(IdType id) noexcept
    {
        return *Slot::atId(id);
    }

    [[nodiscard]] T* slotAt(IdType id) const noexcept
    {
        return const_cast<ObjReg*>(this)->slotAt(id);
    }

    [[nodiscard]] IdType firstFreeFrom(IdType fromId) const noexcept
    {
        IdType start = fromId;
        if (!idInRange(fromId)) {
            start = FirstId;
        }
        const IdType end = endId();
        for (IdType id = start; id < end; ++id) {
            if (slotAt(id) == nullptr) {
                return id;
            }
        }
        return end;
    }

    IdType _registered = IdType{};
};

/**
 * Владеющий реестр = Store ячеек T* с базой ObjReg.
 *
 *   mem::ObjStore&lt;Axis, uint8_t, 16&gt; store;
 *   // ≡ Store&lt;Axis*, uint8_t, 16, ObjReg&lt;Axis&gt;&gt;
 */
template <typename T, typename IdType = uint8_t, IdType Capacity = 16, IdType FirstId = IdType{}>
using ObjStore = Store<T*, IdType, Capacity, ObjReg<T, IdType, FirstId>>;

} // namespace mem
