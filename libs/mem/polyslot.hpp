/**
 * @file mem/polyslot.hpp
 * @brief mem::IPolySlot / PolyStore — однородный полиморфный слот-буфер.
 *
 * Все ячейки — один Derived; снаружи вид Base* с шагом sizeof(Derived).
 * Не смешивает разные наследники в одном массиве.
 *
 *   #include "mem/polyslot.hpp"   // -I …/libs
 *
 *   mem::PolyStore8<Session, ShowSession, 4> bank(node);
 *   Session* s = bank.at(0);
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace mem {

/**
 * Вид на буфер однотипных Derived как на Base[capacity] с шагом stride.
 * Не владеет памятью. Арифметика только по байтам + stride (не Base*).
 *
 * @tparam Base  базовый тип слота (обычно полиморфный)
 * @tparam SizeT тип индексов / ёмкости
 */
template <typename Base, typename SizeT = uint16_t>
class IPolySlot {
public:
    static_assert(std::is_integral_v<SizeT> && std::is_unsigned_v<SizeT>,
                  "IPolySlot: SizeT must be unsigned integral");

    using value_type = Base;
    using cap_t = SizeT;
    using idx_t = cap_t;

    /**
     * @param data     начало массива Derived (как void*)
     * @param capacity число слотов
     * @param stride   sizeof(Derived); должен быть ≥ sizeof(Base)
     */
    constexpr IPolySlot(void* data, cap_t capacity, std::size_t stride) noexcept
        : _data(static_cast<unsigned char*>(data))
        , _capacity(capacity)
        , _stride(stride)
    {}

    ~IPolySlot() = default;

    IPolySlot(const IPolySlot&) = delete;
    IPolySlot& operator=(const IPolySlot&) = delete;

    [[nodiscard]] constexpr cap_t capacity() const noexcept { return _capacity; }
    [[nodiscard]] constexpr std::size_t stride() const noexcept { return _stride; }

    [[nodiscard]] constexpr void* data() noexcept { return _data; }
    [[nodiscard]] constexpr const void* data() const noexcept { return _data; }

    /** Слот @a i как Base*; вне [0, capacity) → nullptr. */
    [[nodiscard]] Base* at(idx_t i) noexcept
    {
        if (_data == nullptr || i >= _capacity || _stride == 0u) {
            return nullptr;
        }
        return reinterpret_cast<Base*>(_data + static_cast<std::size_t>(i) * _stride);
    }

    [[nodiscard]] const Base* at(idx_t i) const noexcept
    {
        return const_cast<IPolySlot*>(this)->at(i);
    }

    /**
     * Обход: fn(Base&, index). Если fn → bool, true — остановить.
     */
    template <typename Fn>
    void forEach(Fn&& fn) noexcept(noexcept(fn(std::declval<Base&>(), idx_t{})))
    {
        for (idx_t i = 0u; i < _capacity; ++i) {
            Base* const p = at(i);
            if (p == nullptr) {
                return;
            }
            if constexpr (std::is_invocable_r_v<bool, Fn&, Base&, idx_t>) {
                if (fn(*p, i)) {
                    return;
                }
            } else {
                fn(*p, i);
            }
        }
    }

    template <typename Fn>
    void forEach(Fn&& fn) const
        noexcept(noexcept(fn(std::declval<const Base&>(), idx_t{})))
    {
        for (idx_t i = 0u; i < _capacity; ++i) {
            const Base* const p = at(i);
            if (p == nullptr) {
                return;
            }
            if constexpr (std::is_invocable_r_v<bool, Fn&, const Base&, idx_t>) {
                if (fn(*p, i)) {
                    return;
                }
            } else {
                fn(*p, i);
            }
        }
    }

protected:
    unsigned char* const _data;
    const cap_t _capacity;
    const std::size_t _stride;
};

/**
 * Владеет Derived[Capacity], снаружи — IPolySlot&lt;Base&gt; (шаг = sizeof(Derived)).
 *
 *   PolyStore&lt;Session, ShowSession, uint8_t, 4&gt; bank(node);
 *
 * @tparam Base      базовый вид
 * @tparam Derived   хранимый тип (is_base_of Base)
 * @tparam SizeT     индексы
 * @tparam Capacity  число слотов (&gt; 0)
 */
template <typename Base, typename Derived, typename SizeT, SizeT Capacity>
class PolyStore : public IPolySlot<Base, SizeT> {
    static_assert(Capacity > 0u, "PolyStore: Capacity > 0");
    static_assert(std::is_base_of_v<Base, Derived>,
                  "PolyStore: Derived must inherit Base");
    static_assert(sizeof(Derived) >= sizeof(Base),
                  "PolyStore: sizeof(Derived) >= sizeof(Base)");

    using PolyBase = IPolySlot<Base, SizeT>;

    template <std::size_t... I, typename... Args>
    PolyStore(std::index_sequence<I...>, Args&... args) noexcept(
        std::is_nothrow_constructible_v<Derived, Args&...>)
        : PolyBase(static_cast<void*>(_cells), Capacity, sizeof(Derived))
        , _cells{((void)I, Derived{args...})...}
    {}

public:
    using value_type = Derived;
    using base_type = Base;
    using cap_t = SizeT;
    using idx_t = SizeT;

    static constexpr SizeT kCapacity = Capacity;

    template <typename... Args>
    explicit PolyStore(Args&... args) noexcept(
        std::is_nothrow_constructible_v<Derived, Args&...>)
        : PolyStore(std::make_index_sequence<Capacity>{}, args...)
    {}

    ~PolyStore() = default;

    PolyStore(const PolyStore&) = delete;
    PolyStore& operator=(const PolyStore&) = delete;

    using PolyBase::at;
    using PolyBase::capacity;
    using PolyBase::forEach;
    using PolyBase::stride;

    /** Тот же слот как Derived*. */
    [[nodiscard]] Derived* atDerived(idx_t i) noexcept
    {
        return static_cast<Derived*>(this->at(i));
    }

    [[nodiscard]] const Derived* atDerived(idx_t i) const noexcept
    {
        return static_cast<const Derived*>(this->at(i));
    }

    [[nodiscard]] Derived& operator[](idx_t i) noexcept { return *atDerived(i); }
    [[nodiscard]] const Derived& operator[](idx_t i) const noexcept
    {
        return *atDerived(i);
    }

private:
    alignas(Derived) Derived _cells[Capacity];
};

template <typename Base, typename Derived, uint8_t Capacity>
using PolyStore8 = PolyStore<Base, Derived, uint8_t, Capacity>;

template <typename Base, typename Derived, uint16_t Capacity>
using PolyStore16 = PolyStore<Base, Derived, uint16_t, Capacity>;

} // namespace mem
