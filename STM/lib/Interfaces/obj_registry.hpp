#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

/**
 * @file obj_registry.hpp
 *
 * `ObjRegistry<T, Id>` — логика регистрации (не создавать по значению; только ссылка/указатель).
 * `ObjStorage<T, Capacity, Id, FirstId>` — конкретное хранилище слотов, можно как поле класса.
 */

namespace MISC {

enum class RegStatus : uint8_t {
    Ok,
    NullEntry,
    IdOutOfRange,
    SlotOccupied,
    NoFreeSlot,
    IdNotSequential,
    SwappedWithOccupiedSlot,
};

template <typename T, typename Id = uint8_t>
class ObjRegistry {
    static_assert(!std::is_pointer_v<T>, "ObjRegistry: T is the pointee type, not T*");
    static_assert(std::is_integral_v<Id>, "ObjRegistry: Id must be an integral type");

protected:
    ObjRegistry() = default;
    virtual ~ObjRegistry() = default;
    ObjRegistry(const ObjRegistry&) = delete;
    ObjRegistry& operator=(const ObjRegistry&) = delete;
    ObjRegistry(ObjRegistry&&) = delete;
    ObjRegistry& operator=(ObjRegistry&&) = delete;

    Id _registered = Id{};

    virtual size_t capacity() const noexcept = 0;
    virtual Id firstId() const noexcept = 0;
    virtual T*& slotAt(Id id) noexcept = 0;

    T* slotAt(Id id) const { return const_cast<ObjRegistry*>(this)->slotAt(id); }

    [[nodiscard]] Id endId() const noexcept {
        return static_cast<Id>(static_cast<size_t>(firstId()) + capacity());
    }

    [[nodiscard]] bool idInRange(Id id) const noexcept {
        if (id < firstId())
            return false;
        return static_cast<size_t>(id - firstId()) < capacity();
    }

    [[nodiscard]] size_t slotIndex(Id id) const noexcept {
        return static_cast<size_t>(id - firstId());
    }

    /** Первый свободный id начиная с `fromId`, или `endId()` если нет. */
    [[nodiscard]] Id firstFreeFrom(Id fromId) const noexcept {
        Id start = fromId;
        if (!idInRange(fromId))
            start = firstId();
        const Id end = endId();
        for (Id id = start; id < end; ++id) {
            if (slotAt(id) == nullptr)
                return id;
        }
        return end;
    }

public:
    [[nodiscard]] Id registeredCount() const noexcept { return _registered; }

    /**
     * Следующий занятый слот после `@p id`; обновляет `id` и возвращает указатель.
     * Перед циклом: `id = 0` (при `firstId() > 0`). По окончании — `nullptr`, `id` — последний выданный.
     */
    [[nodiscard]] T* next(Id& id) noexcept {
        Id cur = firstId();
        if (id >= firstId())
            cur = static_cast<Id>(static_cast<size_t>(id) + 1u);

        const Id end = endId();
        for (; cur < end; ++cur) {
            if (T* const e = slotAt(cur)) {
                id = cur;
                return e;
            }
        }
        return nullptr;
    }

    bool isEmpty(Id id) const noexcept {
        if (!idInRange(id))
            return true;
        return slotAt(id) == nullptr;
    }

    bool contains(Id id) const noexcept { return !isEmpty(id); }

    Id firstFreeId() const noexcept { return firstFreeId(firstId()); }

    Id firstFreeId(Id fromId) const noexcept {
        const Id id = firstFreeFrom(fromId);
        return id < endId() ? id : endId();
    }

    [[nodiscard]] T* get(Id id) noexcept {
        if (!idInRange(id))
            return nullptr;
        return slotAt(id);
    }

    [[nodiscard]] RegStatus registerAt(Id id, T* entry) noexcept {
        if (entry == nullptr)
            return RegStatus::NullEntry;

        if (!idInRange(id))
            return RegStatus::IdOutOfRange;

        T*& cell = slotAt(id);
        if (cell != nullptr) {
            if (cell == entry)
                return RegStatus::Ok;
            return RegStatus::SlotOccupied;
        }

        cell = entry;
        ++_registered;
        entry->set_id(id);
        return RegStatus::Ok;
    }

    [[nodiscard]] RegStatus registerAuto(T* entry, Id& assignedId) noexcept {
        return registerAuto(entry, assignedId, firstId());
    }

    [[nodiscard]] RegStatus registerAuto(T* entry, Id& assignedId, Id fromId) noexcept {
        if (entry == nullptr)
            return RegStatus::NullEntry;

        const Id id = firstFreeFrom(fromId);
        if (id >= endId())
            return RegStatus::NoFreeSlot;

        slotAt(id) = entry;
        ++_registered;
        assignedId = id;
        entry->set_id(assignedId);
        return RegStatus::Ok;
    }

    [[nodiscard]] RegStatus registerSeq(T* entry, Id id) noexcept {
        if (entry == nullptr)
            return RegStatus::NullEntry;

        if (!idInRange(id))
            return RegStatus::IdOutOfRange;

        if (slotIndex(id) != _registered)
            return RegStatus::IdNotSequential;

        return registerAt(id, entry);
    }

    void unregister(Id id) noexcept {
        if (!idInRange(id))
            return;

        T*& cell = slotAt(id);
        if (cell != nullptr) {
            cell = nullptr;
            if (_registered != Id{})
                --_registered;
        }
    }

    void clear() noexcept {
        for (Id id = firstId(); id < endId(); ++id)
            slotAt(id) = nullptr;
        _registered = Id{};
    }

    [[nodiscard]] RegStatus rebind(Id oldId, Id newId) noexcept {
        if (!idInRange(oldId) || !idInRange(newId))
            return RegStatus::IdOutOfRange;

        T* const entry = slotAt(oldId);
        if (entry == nullptr)
            return RegStatus::NullEntry;

        if (oldId == newId)
            return RegStatus::Ok;

        T*& other = slotAt(newId);
        if (other == nullptr) {
            slotAt(oldId) = nullptr;
            slotAt(newId) = entry;
            entry->set_id(newId);
            return RegStatus::Ok;
        }

        if (other == entry)
            return RegStatus::Ok;

        slotAt(oldId) = other;
        slotAt(newId) = entry;
        entry->set_id(newId);
        other->set_id(oldId);
        return RegStatus::SwappedWithOccupiedSlot;
    }
};

template <typename T, size_t Capacity, typename Id = uint8_t, Id FirstId = Id{}>
class ObjStorage : public ObjRegistry<T, Id> {
    static_assert(Capacity > 0u, "ObjStorage: Capacity must be > 0");

    T* _slots[Capacity]{};

public:
    ObjStorage() = default;

protected:
    size_t capacity() const noexcept override { return Capacity; }
    Id firstId() const noexcept override { return FirstId; }
    T*& slotAt(Id id) noexcept override { return _slots[this->slotIndex(id)]; }

public:
    static constexpr size_t staticCapacity() noexcept { return Capacity; }
    static constexpr Id staticFirstId() noexcept { return FirstId; }
};

inline const char* toStr(RegStatus v) noexcept {
    switch (v) {
    case RegStatus::Ok: return "Ok";
    case RegStatus::NullEntry: return "NullEntry";
    case RegStatus::IdOutOfRange: return "IdOutOfRange";
    case RegStatus::SlotOccupied: return "SlotOccupied";
    case RegStatus::NoFreeSlot: return "NoFreeSlot";
    case RegStatus::IdNotSequential: return "IdNotSequential";
    case RegStatus::SwappedWithOccupiedSlot: return "SwappedWithOccupiedSlot";
    default: return "?";
    }
}

} // namespace MISC
