/**
 * @file mem/slot.hpp
 * @brief mem::ISlot / IdSlot / Store / SlotStore — буфер одинаковых слотов.
 *
 * ISlot  — физический вид: индексы 0…capacity-1.
 * IdSlot — ISlot + логические id с First (id = First + index).
 * Store  — владеет T[Capacity], наследует SlotBase (по умолчанию ISlot).
 *
 *   #include "mem/slot.hpp"   // -I …/libs
 *
 *   mem::SlotStore16<Profile, 64> s;
 *   mem::Store<uint32_t, uint16_t, 8> buf;
 *   mem::Store<Axis*, uint8_t, 16, mem::IdSlot<Axis*, uint8_t, 0>> id_buf;
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace mem {

/**
 * Вид на буфер слотов T[capacity]: не владеет памятью.
 *
 * @tparam T     тип слота (нужен move-assign для move())
 * @tparam SizeT тип индексов / ёмкости (uint16_t по умолчанию)
 */
template <typename T, typename SizeT = uint16_t>
class ISlot {
public:
    static_assert(std::is_integral_v<SizeT> && std::is_unsigned_v<SizeT>,
                  "ISlot: SizeT must be unsigned integral");

    using value_type = T;
    /** Ёмкость / счётчики (= SizeT); снаружи: `ISlot<T,U>::cap_t`. */
    using cap_t = SizeT;
    /** Индекс слота; тот же SizeT, завязан на cap_t. */
    using idx_t = cap_t;

    /**
     * Обернуть внешний буфер (не владеет).
     * data == nullptr допустим только при capacity == 0.
     */
    constexpr ISlot(T* data, cap_t capacity) noexcept
        : _data(data)
        , _capacity(capacity)
    {}

    ~ISlot() = default;

    ISlot(const ISlot&) = delete;
    ISlot& operator=(const ISlot&) = delete;

    [[nodiscard]] constexpr cap_t capacity() const noexcept { return _capacity; }

    [[nodiscard]] constexpr T* data() noexcept { return _data; }
    [[nodiscard]] constexpr const T* data() const noexcept { return _data; }

    /** Слот @a i; вне [0, capacity) → nullptr. */
    [[nodiscard]] constexpr T* at(idx_t i) noexcept
    {
        return (_data != nullptr && i < _capacity) ? (_data + i) : nullptr;
    }

    [[nodiscard]] constexpr const T* at(idx_t i) const noexcept
    {
        return const_cast<ISlot*>(this)->at(i);
    }

    /**
     * Обход слотов: fn(T&, index). Если fn → bool, true — остановить.
     */
    template <typename Fn>
    constexpr void forEach(Fn&& fn)
        noexcept(noexcept(fn(std::declval<T&>(), idx_t{})))
    {
        if (_data == nullptr) {
            return;
        }
        for (idx_t i = 0u; i < _capacity; ++i) {
            if constexpr (std::is_invocable_r_v<bool, Fn&, T&, idx_t>) {
                if (fn(_data[i], i)) {
                    return;
                }
            } else {
                fn(_data[i], i);
            }
        }
    }

    template <typename Fn>
    constexpr void forEach(Fn&& fn) const
        noexcept(noexcept(fn(std::declval<const T&>(), idx_t{})))
    {
        if (_data == nullptr) {
            return;
        }
        for (idx_t i = 0u; i < _capacity; ++i) {
            if constexpr (std::is_invocable_r_v<bool, Fn&, const T&, idx_t>) {
                if (fn(_data[i], i)) {
                    return;
                }
            } else {
                fn(_data[i], i);
            }
        }
    }

    /**
     * Перенос count слотов из src в dst (перекрытие допустимо).
     * Цикл move-assign; вызывающий: dst/src/count в пределах capacity.
     */
    void move(idx_t dst, idx_t src, cap_t count) noexcept
    {
        if (_data == nullptr || count == 0u || dst == src) {
            return;
        }
        if (dst < src) {
            for (idx_t i = 0u; i < count; ++i) {
                _data[static_cast<idx_t>(dst + i)] =
                    std::move(_data[static_cast<idx_t>(src + i)]);
            }
        } else {
            for (idx_t i = count; i > 0u; ) {
                --i;
                _data[static_cast<idx_t>(dst + i)] =
                    std::move(_data[static_cast<idx_t>(src + i)]);
            }
        }
    }

protected:
    T* const _data;
    const cap_t _capacity;
};

/**
 * ISlot + нумерация с First: логический id ∈ [First, First+capacity).
 *
 * Физические at/move — protected (индексы 0…capacity-1).
 * Снаружи — firstId/endId/atId/idInRange. ctor как у ISlot: (T*, capacity).
 *
 *   IdSlot&lt;Profile, uint16_t, 10&gt;  // id 10…10+cap-1
 */
template <typename T, typename SizeT = uint16_t, SizeT First = SizeT{}>
class IdSlot : protected ISlot<T, SizeT> {
    using Base = ISlot<T, SizeT>;

public:
    using value_type = T;
    using cap_t = SizeT;
    using idx_t = cap_t;

    static constexpr SizeT kFirst = First;

    constexpr IdSlot(T* data, SizeT capacity) noexcept
        : Base(data, capacity)
    {}

    using Base::capacity;

    [[nodiscard]] static constexpr SizeT firstId() noexcept { return First; }

    [[nodiscard]] constexpr SizeT endId() const noexcept
    {
        return static_cast<SizeT>(static_cast<std::size_t>(First)
                                  + static_cast<std::size_t>(this->capacity()));
    }

    [[nodiscard]] constexpr bool idInRange(SizeT id) const noexcept
    {
        if (id < First) {
            return false;
        }
        return static_cast<std::size_t>(id - First)
            < static_cast<std::size_t>(this->capacity());
    }

    /** Физический индекс; только при idInRange. */
    [[nodiscard]] static constexpr SizeT indexOf(SizeT id) noexcept
    {
        return static_cast<SizeT>(id - First);
    }

    /** Слот по логическому id; вне диапазона → nullptr. */
    [[nodiscard]] constexpr T* atId(SizeT id) noexcept
    {
        return idInRange(id) ? Base::at(indexOf(id)) : nullptr;
    }

    [[nodiscard]] constexpr const T* atId(SizeT id) const noexcept
    {
        return const_cast<IdSlot*>(this)->atId(id);
    }

protected:
    using Base::at;
    using Base::data;
    using Base::forEach;
    using Base::move;
};

/**
 * Владеет T[Capacity], наследует SlotBase (по умолчанию ISlot&lt;T, SizeT&gt;).
 *
 * Indexed=false: SlotBase(cells, cap); T{args...} (пустой pack → T{}).
 * Indexed=true:  SlotBase(args..., cells, cap); T{SlotBase&, id}
 *   (ячейка знает базу-хранилище; args — доп. аргументы базы, напр. payload).
 *
 *   Store&lt;uint32_t, uint16_t, 8&gt; a;
 *   Store&lt;AllocRange&lt;…&gt;, …, N, IRangeAllocator&lt;…&gt;, true&gt; ranges(payload);
 *
 * @tparam T         тип слота
 * @tparam SizeT     тип индексов
 * @tparam Capacity  число слотов (&gt; 0)
 * @tparam SlotBase  база-вид/адаптер (ISlot / IdSlot / IRangeAllocator)
 * @tparam Indexed   true → T{SlotBase&, SizeT(i)}
 */
template <typename T,
          typename SizeT,
          SizeT Capacity,
          typename SlotBase = ISlot<T, SizeT>,
          bool Indexed = false>
class Store : public SlotBase {
    static_assert(Capacity > 0u, "Store: Capacity > 0");

    /** Indexed: SlotBase(args..., cells, cap); T{SlotBase&, id}. */
    template <std::size_t... I, typename... Args>
    Store(std::true_type, std::index_sequence<I...>, Args&... args)
        noexcept(std::is_nothrow_constructible_v<SlotBase, Args&..., T*, SizeT>
                 && std::is_nothrow_constructible_v<T, SlotBase&, SizeT>)
        : SlotBase(args..., _cells, Capacity)
        , _cells{T{static_cast<SlotBase&>(*this), static_cast<SizeT>(I)}...}
    {}

    template <std::size_t... I, typename... Args>
    Store(std::false_type, std::index_sequence<I...>, Args&... args)
        noexcept(std::is_nothrow_constructible_v<T, Args&...>)
        : SlotBase(_cells, Capacity)
        , _cells{((void)I, T{args...})...}
    {}

public:
    using value_type = T;
    using cap_t = SizeT;
    using idx_t = cap_t;
    using Base = SlotBase;

    static constexpr SizeT kCapacity = Capacity;
    static constexpr bool kIndexed = Indexed;

    template <typename... Args>
    explicit Store(Args&... args) noexcept(
        Indexed ? (std::is_nothrow_constructible_v<SlotBase, Args&..., T*, SizeT>
                   && std::is_nothrow_constructible_v<T, SlotBase&, SizeT>)
                : std::is_nothrow_constructible_v<T, Args&...>)
        : Store(std::bool_constant<Indexed>{},
                std::make_index_sequence<Capacity>{},
                args...)
    {}

    ~Store() = default;

    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

private:
    T _cells[Capacity];
};

template <typename T, typename SizeT, SizeT Capacity>
using SlotStore = Store<T, SizeT, Capacity>;

template <typename T, uint16_t Capacity>
using SlotStore16 = Store<T, uint16_t, Capacity>;

template <typename T, uint8_t Capacity>
using SlotStore8 = Store<T, uint8_t, Capacity>;

template <typename T, typename SizeT, SizeT Capacity, typename SlotBase = ISlot<T, SizeT>>
using IdStore = Store<T, SizeT, Capacity, SlotBase, true>;

} // namespace mem
