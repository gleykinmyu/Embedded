/**
 * @file mem/range_alloc.hpp
 * @brief mem::Range, SlotRange, AllocRange, IRangeAllocator / RangeAllocator.
 *
 *   #include "mem/range_alloc.hpp"   // -I …/libs
 *
 *   mem::SlotStore16<Profile, 64> slots;
 *   mem::RangeAllocator16<Profile, 16> ranges(slots);
 *   auto* r = ranges.alloc(3);
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "mem/slot.hpp"

namespace mem {


template <typename T, typename SizeT = uint16_t>
class IRangeAllocator;

template <typename T, typename SizeT>
class AllocRange;

/**
 * Диапазон [first, first + count) — дескриптор, без привязки к буферу.
 */
template <typename SizeT = uint16_t>
struct Range {
    static_assert(std::is_integral_v<SizeT> && std::is_unsigned_v<SizeT>,
                  "Range: SizeT must be unsigned integral");

    using cap_t = SizeT;
    using idx_t = cap_t;

    SizeT first = 0;
    SizeT count = 0;

    [[nodiscard]] constexpr bool empty() const noexcept { return count == 0u; }

    [[nodiscard]] constexpr SizeT end() const noexcept
    {
        return static_cast<SizeT>(first + count);
    }
};

static_assert(sizeof(Range<uint16_t>) == 4u);
static_assert(std::is_standard_layout_v<Range<uint16_t>>);
static_assert(std::is_trivially_copyable_v<Range<uint16_t>>);

/**
 * Вид на непрерывный отрезок слотов ISlot&lt;T&gt; по дескриптору Range.
 */
template <typename T, typename SizeT = uint16_t>
class SlotRange {
    static_assert(std::is_integral_v<SizeT> && std::is_unsigned_v<SizeT>,
                  "SlotRange: SizeT must be unsigned integral");

public:
    using value_type = T;
    using cap_t = SizeT;
    using idx_t = cap_t;
    using range_type = Range<SizeT>;
    using slot_type = ISlot<T, SizeT>;

    constexpr SlotRange(slot_type& slots, SizeT first, SizeT count) noexcept
        : _slots(slots)
        , _range{first, count}
    {}

    constexpr SlotRange(slot_type& slots, range_type range) noexcept
        : _slots(slots)
        , _range(range)
    {}

    SlotRange(const SlotRange&) = default;
    SlotRange& operator=(const SlotRange&) = delete;

    [[nodiscard]] constexpr slot_type& slots() noexcept { return _slots; }
    [[nodiscard]] constexpr const slot_type& slots() const noexcept { return _slots; }

    [[nodiscard]] constexpr range_type range() const noexcept { return _range; }
    [[nodiscard]] constexpr cap_t size() const noexcept { return _range.count; }
    [[nodiscard]] constexpr bool empty() const noexcept { return _range.empty(); }
    [[nodiscard]] constexpr SizeT end() const noexcept { return _range.end(); }

    /** Указатель на первый слот диапазона; пустой → nullptr. */
    [[nodiscard]] constexpr T* data() noexcept
    {
        return empty() ? nullptr : _slots.at(_range.first);
    }

    [[nodiscard]] constexpr const T* data() const noexcept
    {
        return const_cast<SlotRange*>(this)->data();
    }

    /** Слот i внутри диапазона (0…count-1); вне → nullptr. */
    [[nodiscard]] constexpr T* at(SizeT i) noexcept
    {
        if (i >= _range.count) {
            return nullptr;
        }
        return _slots.at(static_cast<SizeT>(_range.first + i));
    }

    [[nodiscard]] constexpr const T* at(SizeT i) const noexcept
    {
        return const_cast<SlotRange*>(this)->at(i);
    }

    /**
     * Обход слотов диапазона: fn(T&, index). Если fn → bool, true — остановить.
     * at(i) == nullptr — пропуск.
     */
    template <typename Fn>
    constexpr void forEach(Fn&& fn)
        noexcept(noexcept(fn(std::declval<T&>(), idx_t{})))
    {
        const cap_t n = size();
        for (idx_t i = 0u; i < n; ++i) {
            T* const p = at(i);
            if (p == nullptr) {
                continue;
            }
            if constexpr (std::is_invocable_r_v<bool, Fn&, T&, idx_t>) {
                if (fn(*p, i)) {
                    return;
                }
            } else {
                fn(*p, i);
            }
        }
    }

protected:
    void setRange(range_type range) noexcept { _range = range; }

    void clearRange() noexcept
    {
        _range.first = 0u;
        _range.count = 0u;
    }

    slot_type& _slots;
    range_type _range;
};

/**
 * База аллокатора диапазонов: без Capacity в типе.
 * Сам аллокатор = ISlot&lt;AllocRange&gt;; payload — отдельный ISlot&lt;T&gt;.
 */
template <typename T, typename SizeT>
class IRangeAllocator : public ISlot<AllocRange<T, SizeT>, SizeT> {
    static_assert(std::is_integral_v<SizeT> && std::is_unsigned_v<SizeT>,
                  "IRangeAllocator: SizeT must be unsigned integral");

    using ItemSlot = ISlot<AllocRange<T, SizeT>, SizeT>;

public:
    using value_type = T;
    using cap_t = SizeT;
    using idx_t = cap_t;
    using range_type = Range<SizeT>;
    using item_type = AllocRange<T, SizeT>;
    using slot_type = ISlot<T, SizeT>;

    using ItemSlot::at;
    using ItemSlot::capacity;

    /**
     * @param slots    буфер полезной нагрузки
     * @param items    массив AllocRange (элементы конструирует владелец)
     * @param capacity число элементов items
     */
    IRangeAllocator(slot_type& slots, item_type* items, SizeT capacity) noexcept
        : ItemSlot(items, capacity)
        , _slots(slots)
    {}

    IRangeAllocator(const IRangeAllocator&) = delete;
    IRangeAllocator& operator=(const IRangeAllocator&) = delete;

    [[nodiscard]] SizeT freeTop() const noexcept { return _free_top; }
    [[nodiscard]] SizeT freeSlots() const noexcept
    {
        return static_cast<SizeT>(_slots.capacity() - _free_top);
    }

    /**
     * Свободный дескриптор + resize(n). n == 0 → nullptr.
     * Нет дескриптора / payload — nullptr (дескриптор не остаётся занятым).
     */
    [[nodiscard]] item_type* alloc(SizeT n) noexcept
    {
        if (n == 0u) {
            return nullptr;
        }
        item_type* p = nullptr;
        this->forEach([&](item_type& it, SizeT) -> bool {
            if (it.isFree()) {
                p = &it;
                return true; /* стоп */
            }
            return false;
        });
        if (p == nullptr) {
            return nullptr;
        }
        if (!p->resize(n)) {
            p->free();
            return nullptr;
        }
        return p;
    }

    /** Вернуть дескриптор в пул. false: nullptr или чужой allocator. */
    bool free(item_type* p) noexcept
    {
        if (p == nullptr || &p->allocator() != this) {
            return false;
        }
        p->free();
        return true;
    }

    [[nodiscard]] bool isLayoutOk() const noexcept;

    void compact() noexcept;

private:
    friend class AllocRange<T, SizeT>;

    void setFreeTop(SizeT v) noexcept { _free_top = v; }

    slot_type& _slots;
    SizeT _free_top = 0;
};

/**
 * Аллоцируемый диапазон: SlotRange + ссылка на IRangeAllocator (resize / free / compact).
 */
template <typename T, typename SizeT = uint16_t>
class AllocRange : public SlotRange<T, SizeT> {
    using Base = SlotRange<T, SizeT>;
    using allocator_type = IRangeAllocator<T, SizeT>;

    friend class IRangeAllocator<T, SizeT>;

public:
    using typename Base::range_type;
    using typename Base::cap_t;
    using typename Base::idx_t;
    using typename Base::slot_type;
    using typename Base::value_type;

    /** SizeT — индекс от Store Indexed; в объекте не хранится. */
    AllocRange(allocator_type& alloc, SizeT /*index*/) noexcept
        : Base(alloc._slots, range_type{})
        , _alloc(alloc)
    {}

    AllocRange(const AllocRange&) = delete;
    AllocRange& operator=(const AllocRange&) = delete;

    using Base::at;
    using Base::data;
    using Base::empty;
    using Base::end;
    using Base::forEach;
    using Base::range;
    using Base::slots;

    /** Свободен для alloc: пустой дескриптор. */
    [[nodiscard]] bool isFree() const noexcept { return this->empty(); }

    [[nodiscard]] allocator_type& allocator() noexcept { return _alloc; }
    [[nodiscard]] const allocator_type& allocator() const noexcept { return _alloc; }

    /**
     * Выставить число слотов в n.
     * При нехватке хвоста — compact и повтор.
     * @return false если места всё равно нет (или range битый).
     */
    [[nodiscard]] bool resize(SizeT n) noexcept
    {
        if (resizeOnce(n)) {
            return true;
        }
        compact();
        return resizeOnce(n);
    }

    /** Сбросить дескриптор (ячейки не трогаем) → isFree. */
    void free() noexcept
    {
        this->_range.first = 0u;
        this->_range.count = 0u;
    }

    void compact() noexcept { _alloc.compact(); }

private:
    [[nodiscard]] bool resizeOnce(SizeT n) noexcept;

    allocator_type& _alloc;
};

template <typename T, typename SizeT>
bool AllocRange<T, SizeT>::resizeOnce(SizeT n) noexcept
{
    range_type& pr = this->_range;
    typename Base::slot_type& sl = this->_slots;

    if (n == pr.count) {
        return true;
    }

    if (n < pr.count) {
        pr.count = n;
        if (n == 0u) {
            pr.first = 0u;
        }
        return true;
    }

    const SizeT cap = sl.capacity();
    SizeT free_top = _alloc.freeTop();

    if (pr.count == 0u) {
        if (n > static_cast<SizeT>(cap - free_top)) {
            return false;
        }
        pr.first = free_top;
        pr.count = n;
        _alloc.setFreeTop(static_cast<SizeT>(free_top + n));
        return true;
    }

    if (pr.first >= cap || pr.count > static_cast<SizeT>(cap - pr.first)) {
        return false;
    }

    if (pr.end() == free_top) {
        const SizeT need = static_cast<SizeT>(n - pr.count);
        if (need > static_cast<SizeT>(cap - free_top)) {
            return false;
        }
        pr.count = n;
        _alloc.setFreeTop(static_cast<SizeT>(free_top + need));
        return true;
    }

    if (n > static_cast<SizeT>(cap - free_top)) {
        return false;
    }
    const SizeT src_first = pr.first;
    const SizeT src_count = pr.count;
    const SizeT dst_first = free_top;
    sl.move(dst_first, src_first, src_count);
    pr.first = dst_first;
    pr.count = n;
    _alloc.setFreeTop(static_cast<SizeT>(free_top + n));
    return true;
}

template <typename T, typename SizeT>
void IRangeAllocator<T, SizeT>::compact() noexcept
{
    const SizeT cap = _slots.capacity();

    this->forEach([&](item_type& it, SizeT) {
        range_type& pr = it._range;
        if (pr.count == 0u) {
            pr.first = 0u;
            return;
        }
        if (pr.first >= cap || pr.count > static_cast<SizeT>(cap - pr.first)) {
            pr.first = 0u;
            pr.count = 0u;
        }
    });

    SizeT dst = 0u;
    for (;;) {
        bool found = false;
        SizeT best = 0u;
        SizeT best_first = 0u;
        this->forEach([&](item_type& it, SizeT i) {
            const range_type& pr = it._range;
            if (pr.count == 0u || pr.first < dst) {
                return;
            }
            if (!found || pr.first < best_first) {
                found = true;
                best = i;
                best_first = pr.first;
            }
        });
        if (!found) {
            break;
        }
        range_type& pr = this->at(best)->_range;
        if (pr.first != dst) {
            _slots.move(dst, pr.first, pr.count);
            pr.first = dst;
        }
        dst = static_cast<SizeT>(dst + pr.count);
    }

    _free_top = dst;
}

template <typename T, typename SizeT>
bool IRangeAllocator<T, SizeT>::isLayoutOk() const noexcept
{
    const SizeT cap = _slots.capacity();
    const SizeT rc = this->capacity();
    for (SizeT i = 0u; i < rc; ++i) {
        const item_type* ia = this->at(i);
        if (ia == nullptr || ia->isFree()) {
            continue;
        }
        const range_type& a = ia->range();
        if (a.count == 0u) {
            continue;
        }
        if (a.first >= cap || a.count > static_cast<SizeT>(cap - a.first)) {
            return false;
        }
        for (SizeT j = static_cast<SizeT>(i + 1u); j < rc; ++j) {
            const item_type* ib = this->at(j);
            if (ib == nullptr || ib->isFree()) {
                continue;
            }
            const range_type& b = ib->range();
            if (b.count == 0u) {
                continue;
            }
            if (b.first >= cap || b.count > static_cast<SizeT>(cap - b.first)) {
                return false;
            }
            if ((a.first < b.end()) && (b.first < a.end())) {
                return false;
            }
        }
    }
    return true;
}

/**
 * Владеющий аллокатор диапазонов: Store&lt;AllocRange&gt; с базой IRangeAllocator.
 * Payload (ISlot&lt;T&gt;) передаётся снаружи в ctor.
 *
 *   SlotStore16&lt;T, SlotCap&gt; slots;
 *   RangeAllocator16&lt;T, RangeCap&gt; ranges(slots);
 */
template <typename T, typename SizeT, SizeT RangeCap>
using RangeAllocator = Store<AllocRange<T, SizeT>,
                        SizeT,
                        RangeCap,
                        IRangeAllocator<T, SizeT>,
                        true>;

template <typename T, uint16_t RangeCap>
using RangeAllocator16 = RangeAllocator<T, uint16_t, RangeCap>;

template <typename T, uint8_t RangeCap>
using RangeAllocator8 = RangeAllocator<T, uint8_t, RangeCap>;


} // namespace mem
