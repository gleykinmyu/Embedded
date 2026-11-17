/**
 * @file mem/obj_registry.hpp
 * @brief mem::ObjRegistry — реестр ссылок T* на объекты; объекты регистрируются сами.
 *
 * Режим — параметр шаблона; у каждой подстановки только свои методы:
 *   NoIds<SizeT>                 — объект не знает своего места (протоколы, сессии).
 *   WithIds<IdT, First, NoId>    — объект знает свой id (set_id): оси, компоненты Nextion…
 *                                  id = First + индекс; NoId — «нет id» (вне рабочего диапазона).
 *
 * Регистрация всегда возвращает RegStatus и одновременно запоминает первый сбой
 * (липкий regStatus()): ребёнок в конструкторе результат может игнорировать.
 *
 *   mem::ObjRegStore<IProto, 4> protos;                              // NoIds
 *   mem::ObjRegStore<Axis, 16, mem::WithIds<uint8_t>> axes;          // id 0…15, NoId = 255
 *   struct Proto { explicit Proto(mem::IRegistrar<Proto>& r) { r.registerAuto(*this); } };
 */

#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>

#include "mem/slot.hpp"
#include "mem/visit.hpp"

namespace mem {

/** В теле класса T с приватным set_id: доступ реестру (WithIds). */
#define MEM_OBJ_REGISTRY_FRIEND                                                                    \
    template <typename, typename>                                                                  \
    friend class ::mem::detail::RegApi

enum class RegStatus : uint8_t {
    Ok = 0,
    OutOfRange,  ///< id вне [kFirstId, endId)
    Occupied,    ///< слот занят другим объектом
    NoFreeSlot,  ///< свободных слотов нет
    Duplicate,   ///< объект уже зарегистрирован под другим id
    NotFound,    ///< по id / объекту ничего нет
};

[[nodiscard]] inline const char* cstr(RegStatus s) noexcept
{
    switch (s) {
    case RegStatus::Ok: return "Ok";
    case RegStatus::OutOfRange: return "OutOfRange";
    case RegStatus::Occupied: return "Occupied";
    case RegStatus::NoFreeSlot: return "NoFreeSlot";
    case RegStatus::Duplicate: return "Duplicate";
    case RegStatus::NotFound: return "NotFound";
    }
    return "?";
}

/** Режим: объект не знает своего места; позиция — только порядок обхода. */
template <typename SizeT = uint8_t>
struct NoIds {
    using size_type = SizeT;
};

/** Режим: объект знает свой id (T::set_id(IdT)); NoId сообщается при снятии. */
template <typename IdT = uint8_t,
          IdT First = IdT{},
          IdT NoId = std::numeric_limits<IdT>::max()>
struct WithIds {
    static_assert(NoId < First || NoId == std::numeric_limits<IdT>::max(),
                  "WithIds: NoId must lie outside [First, endId)");
    using size_type = IdT;
    using id_type = IdT;
    static constexpr IdT kFirstId = First;
    static constexpr IdT kNoId = NoId;
};

/** «Запиши меня куда-нибудь» (первый свободный слот). */
template <typename T>
class IRegistrar {
public:
    virtual RegStatus registerAuto(T& obj) noexcept = 0;

protected:
    IRegistrar() = default;
    ~IRegistrar() = default;
    IRegistrar(const IRegistrar&) = delete;
    IRegistrar& operator=(const IRegistrar&) = delete;
};

/** «Запиши меня под этим id». Не зависит от First / ёмкости. */
template <typename T, typename IdT>
class IIdRegistrar {
public:
    virtual RegStatus registerAt(IdT id, T& obj) noexcept = 0;

protected:
    IIdRegistrar() = default;
    ~IIdRegistrar() = default;
    IIdRegistrar(const IIdRegistrar&) = delete;
    IIdRegistrar& operator=(const IIdRegistrar&) = delete;
};

namespace detail {

/** Ядро: ячейки T* (View), счётчик, липкий статус; всё по физическим индексам. */
template <typename T, typename SizeT, typename View>
class RegCore : protected View {
    static_assert(!std::is_pointer_v<T>, "ObjRegistry: T is the pointee type, not T*");

public:
    using value_type = T;
    using size_type = SizeT;
    /** Ёмкость > kMaxCapacity вид обрезает (IdSlot: endId() помещается в IdT). */
    static constexpr SizeT kMaxCapacity = View::kMaxCapacity;

    using View::capacity;
    [[nodiscard]] SizeT count() const noexcept { return _count; }
    [[nodiscard]] bool isFull() const noexcept { return _count >= capacity(); }

    /** Первый сбой регистрации (липкий); Ok — все удались. */
    [[nodiscard]] RegStatus regStatus() const noexcept { return _regStatus; }
    [[nodiscard]] bool isOk() const noexcept { return _regStatus == RegStatus::Ok; }

    [[nodiscard]] bool contains(const T& obj) const noexcept { return findIdx(obj) < capacity(); }

protected:
    RegCore(T** cells, SizeT capacity) noexcept
        : View(cells, cells != nullptr ? capacity : SizeT{})
    {}

    /** Запомнить первый сбой; вернуть @a s как есть. */
    RegStatus note(RegStatus s) noexcept
    {
        if (s != RegStatus::Ok && _regStatus == RegStatus::Ok) {
            _regStatus = s;
        }
        return s;
    }

    [[nodiscard]] T* objAt(SizeT idx) const noexcept
    {
        T* const* const c = View::at(idx);
        return c != nullptr ? *c : nullptr;
    }

    /** Положить в слот @a idx. Тот же объект там же — Ok. */
    [[nodiscard]] RegStatus placeAt(SizeT idx, T& obj) noexcept
    {
        T** const c = View::at(idx);
        if (c == nullptr) {
            return RegStatus::OutOfRange;
        }
        if (*c != nullptr) {
            return *c == &obj ? RegStatus::Ok : RegStatus::Occupied;
        }
        *c = &obj;
        ++_count;
        return RegStatus::Ok;
    }

    /**
     * Один проход: @a obj уже есть — его слот; иначе — положить в первый пустой.
     * @return слот объекта; нет места — capacity().
     */
    [[nodiscard]] SizeT placeAuto(T& obj) noexcept
    {
        SizeT free = capacity();
        SizeT left = _count;
        for (SizeT i = 0; i < capacity(); ++i) {
            T* const c = this->_data[i];
            if (c == nullptr) {
                if (free == capacity()) {
                    free = i;
                }
                if (left == SizeT{}) {
                    break;
                }
                continue;
            }
            if (c == &obj) {
                return i;
            }
            --left;
        }
        if (free < capacity()) {
            this->_data[free] = &obj;
            ++_count;
        }
        return free;
    }

    /** Слот объекта @a obj; нет — capacity(). */
    [[nodiscard]] SizeT findIdx(const T& obj) const noexcept
    {
        SizeT found = capacity();
        each<const T>(*this, [&](const T& e, SizeT i) -> bool {
            if (&e != &obj) {
                return false;
            }
            found = i;
            return true;
        });
        return found;
    }

    /** Освободить слот @a idx. @return false — пуст / вне диапазона. */
    bool removeAt(SizeT idx) noexcept
    {
        T** const c = View::at(idx);
        if (c == nullptr || *c == nullptr) {
            return false;
        }
        *c = nullptr;
        --_count;
        return true;
    }

    void clearAll() noexcept
    {
        for (SizeT i = 0; i < capacity(); ++i) {
            this->_data[i] = nullptr;
        }
        _count = SizeT{};
    }

    /**
     * Обход занятых: fn(E&, idx) → bool (true — стоп).
     * Останавливается после count() занятых; снимать текущий внутри fn можно.
     */
    template <typename E, typename Self, typename Fn>
    static void each(Self& self, Fn&& fn)
    {
        SizeT left = self._count;
        for (SizeT i = 0; i < self.capacity() && left != SizeT{}; ++i) {
            E* const e = self._data[i];
            if (e == nullptr) {
                continue;
            }
            --left;
            if (fn(*e, i)) {
                return;
            }
        }
    }

private:
    SizeT _count = SizeT{};
    RegStatus _regStatus = RegStatus::Ok;
};

template <typename T, typename Mode>
class RegApi;

/** NoIds: множество объектов; позиция — только порядок. */
template <typename T, typename SizeT>
class RegApi<T, NoIds<SizeT>> : public RegCore<T, SizeT, ISlot<T*, SizeT>>,
                                public IRegistrar<T> {
    using Core = RegCore<T, SizeT, ISlot<T*, SizeT>>;

public:
    RegApi(T** cells, SizeT capacity) noexcept
        : Core(cells, capacity)
    {}

    /** Первый свободный слот; уже зарегистрирован — Ok. */
    RegStatus registerAuto(T& obj) noexcept final
    {
        if (Core::placeAuto(obj) >= Core::capacity()) {
            return Core::note(RegStatus::NoFreeSlot);
        }
        return RegStatus::Ok;
    }

    /** @return false — объекта нет. */
    bool unregister(T& obj) noexcept { return Core::removeAt(Core::findIdx(obj)); }

    /** Объект на позиции @a idx; пусто / вне диапазона → nullptr. */
    [[nodiscard]] T* get(SizeT idx) const noexcept { return Core::objAt(idx); }

    /** Обход: fn(T&); fn → bool, true — остановить. */
    template <typename Fn>
    void forEach(Fn&& fn)
    {
        Core::template each<T>(*this, [&](T& e, SizeT) { return detail::visit(fn, e); });
    }
    template <typename Fn>
    void forEach(Fn&& fn) const
    {
        Core::template each<const T>(*this, [&](const T& e, SizeT) { return detail::visit(fn, e); });
    }

    void clear() noexcept { Core::clearAll(); }
};

/** WithIds: объект знает свой id (set_id); id = First + индекс. */
template <typename T, typename IdT, IdT First, IdT NoId>
class RegApi<T, WithIds<IdT, First, NoId>>
    : public RegCore<T, IdT, IdSlot<T*, IdT, First>>,
      public IRegistrar<T>,
      public IIdRegistrar<T, IdT> {
    using View = IdSlot<T*, IdT, First>;
    using Core = RegCore<T, IdT, View>;

public:
    using id_type = IdT;
    using View::kFirstId;
    static constexpr IdT kNoId = NoId;

    RegApi(T** cells, IdT capacity) noexcept
        : Core(cells, capacity)
    {}

    using View::endId;
    using View::idInRange;

    /** Первый свободный id + set_id; уже зарегистрирован — Ok. */
    RegStatus registerAuto(T& obj) noexcept final
    {
        const IdT idx = Core::placeAuto(obj);
        if (idx >= Core::capacity()) {
            return Core::note(RegStatus::NoFreeSlot);
        }
        obj.set_id(View::idOf(idx));
        return RegStatus::Ok;
    }

    /** На id @a id + set_id. Тот же объект там же — Ok; под другим id — Duplicate. */
    RegStatus registerAt(IdT id, T& obj) noexcept final
    {
        if (!idInRange(id)) {
            return Core::note(RegStatus::OutOfRange);
        }
        const IdT idx = View::indexOf(id);
        const IdT was = Core::findIdx(obj);
        if (was < Core::capacity() && was != idx) {
            return Core::note(RegStatus::Duplicate);
        }
        const RegStatus s = Core::placeAt(idx, obj);
        if (s == RegStatus::Ok) {
            obj.set_id(id);
        }
        return Core::note(s);
    }

    /** Снять @a obj; объекту — set_id(kNoId). @return false — его нет. */
    bool unregister(T& obj) noexcept
    {
        if (!Core::removeAt(Core::findIdx(obj))) {
            return false;
        }
        obj.set_id(kNoId);
        return true;
    }

    /** Снять объект с id @a id; объекту — set_id(kNoId). @return false — пусто. */
    bool unregisterAt(IdT id) noexcept
    {
        T* const obj = get(id);
        if (obj == nullptr) {
            return false;
        }
        (void)Core::removeAt(View::indexOf(id));
        obj->set_id(kNoId);
        return true;
    }

    /** Перенести @a oldId → @a newId; занятый newId — обмен (оба получают set_id). */
    RegStatus rebind(IdT oldId, IdT newId) noexcept
    {
        if (!idInRange(oldId) || !idInRange(newId)) {
            return RegStatus::OutOfRange;
        }
        T* const obj = get(oldId);
        if (obj == nullptr) {
            return RegStatus::NotFound;
        }
        if (oldId == newId) {
            return RegStatus::Ok;
        }
        T* const other = get(newId);
        (void)View::swap(View::indexOf(oldId), View::indexOf(newId));
        obj->set_id(newId);
        if (other != nullptr) {
            other->set_id(oldId);
        }
        return RegStatus::Ok;
    }

    /** Объект с id @a id; пусто / вне диапазона → nullptr. */
    [[nodiscard]] T* get(IdT id) const noexcept
    {
        return idInRange(id) ? Core::objAt(View::indexOf(id)) : nullptr;
    }

    /** id объекта @a obj; нет — kNoId. */
    [[nodiscard]] IdT find(const T& obj) const noexcept
    {
        const IdT idx = Core::findIdx(obj);
        return idx < Core::capacity() ? View::idOf(idx) : kNoId;
    }

    /** Обход: fn(T&, id); fn → bool, true — остановить. */
    template <typename Fn>
    void forEach(Fn&& fn)
    {
        Core::template each<T>(*this, [&](T& e, IdT idx) { return detail::visit(fn, e, View::idOf(idx)); });
    }
    template <typename Fn>
    void forEach(Fn&& fn) const
    {
        Core::template each<const T>(*this, [&](const T& e, IdT idx) {
            return detail::visit(fn, e, View::idOf(idx));
        });
    }

    /** Снять все; объектам — set_id(kNoId). */
    void clear() noexcept
    {
        forEach([](T& e, IdT) { e.set_id(kNoId); });
        Core::clearAll();
    }
};

} // namespace detail

/**
 * Реестр ссылок на объекты. Mode: NoIds<SizeT> (по умолчанию) или WithIds<IdT, First, NoId>.
 * Внешний буфер ячеек (обнулён): ObjRegistry(cells, n); владеющий — ObjRegStore.
 */
template <typename T, typename Mode = NoIds<>>
class ObjRegistry : public detail::RegApi<T, Mode> {
    using Api = detail::RegApi<T, Mode>;

public:
    using mode_type = Mode;
    using size_type = typename Mode::size_type;

    ObjRegistry(T** cells, size_type capacity) noexcept
        : Api(cells, capacity)
    {}
};

/**
 *   mem::ObjRegStore<IProto, 4>                          // NoIds<uint8_t>
 *   mem::ObjRegStore<Axis, 16, mem::WithIds<uint8_t>>    // id 0…15
 */
namespace detail {
template <typename Reg, auto Capacity>
constexpr typename Reg::size_type regCapacity() noexcept
{
    static_assert(std::is_integral_v<decltype(Capacity)>, "ObjRegStore: Capacity must be integral");
    static_assert(static_cast<unsigned long long>(Capacity) <= Reg::kMaxCapacity,
                  "ObjRegStore: Capacity exceeds kMaxCapacity");
    return static_cast<typename Reg::size_type>(Capacity);
}
} // namespace detail

template <typename T, auto Capacity, typename Mode = NoIds<>>
using ObjRegStore = Store<T*,
                          typename Mode::size_type,
                          detail::regCapacity<ObjRegistry<T, Mode>, Capacity>(),
                          ObjRegistry<T, Mode>>;

} // namespace mem
