/**
 * @file slot_pool.hpp
 * @brief slot::Range + IPool / View / Pool — блочный аллокатор слотов (не arena).
 *
 * ============================================================================
 * Назначение
 * ============================================================================
 * Фиксированный пул однотипных слотов, разрезанный на непрерывные диапазоны
 * (индексированные диапазоны / range). Типичный кейс — RAM-хранилище профилей
 * осей: у каждой оси свой кусок слотов переменной длины.
 *
 * Модель та же, что у sf::SectionPool, но без шоуфайла и без заголовков Hdr:
 * только массив слотов + таблица Range. Flash/sf-адаптер сможет сесть на
 * тот же IPool, подставив своё хранилище через хуки.
 *
 * ============================================================================
 * Инварианты раскладки
 * ============================================================================
 *   - ёмкость слотов (_slot_capacity) и число диапазонов (_range_count)
 *     задаются при создании и не меняются;
 *   - диапазон i — полуинтервал [first, first+count) в индексах слотов;
 *     count==0 ⇒ пуст (first нормализуется в 0);
 *   - живые диапазоны не пересекаются;
 *   - _free_top — bump-указатель: следующий свободный индекс после «хвоста».
 *     Слоты [0, _free_top) могут содержать дыры (после clear/shrink чужого
 *     диапазона); сплошной префикс без дыр даёт только compact();
 *   - аллокация роста всегда идёт в хвост (от _free_top), либо расширением
 *     диапазона, уже стоящего вплотную к _free_top.
 *
 * ============================================================================
 * Жизненный цикл диапазона
 * ============================================================================
 *   resize(range, n)     — shrink на месте / grow bump или relocate в хвост
 *   clear(range)         — count=0, слоты обнулить; дыра остаётся до compact
 *   compact()            — сдвинуть живые диапазоны к 0, поправить first
 *   resizeCompact(...)   — resize; при отказе — compact и ещё раз
 *   syncFreeTop()        — пересчитать _free_top = max(end) по живым Range
 *                          (нужен после внешней правки таблицы Range)
 *
 * После compact() чужой View остаётся валидным: перед каждым доступом
 * он заново читает свой Range.
 *
 * ============================================================================
 * Архитектура (NVI)
 * ============================================================================
 * IPool<SizeT>  — вся логика bump/compact/layout + type-erasure слота
 *                      (байты + stride). Наследник только отдаёт хранилище.
 * Pool<...>     — RAM: T[Capacity] + Range[MaxRanges].
 * View<T,SizeT>  — лёгкий вид на один диапазон.
 *
 * Хранимый тип T обязан быть standard_layout + trivially_copyable:
 * перенос — memmove, обнуление — memset (как у POD-записей пула).
 *
 * ============================================================================
 * SizeT
 * ============================================================================
 * Тип индексов и счётчиков (first, count, ёмкости). По умолчанию uint16_t.
 * Для крошечных пулов — uint8_t; для очень больших — uint32_t.
 * Pool16<T,Cap,Ranges> — алиас с SizeT = uint16_t.
 *
 * ============================================================================
 * Пример
 * ============================================================================
 *   using MyPool = slot::Pool16<Profile, 64, 16>;
 *   MyPool p;
 *   p.resizeCompact(0, 3);          // диапазон 0 → 3 слота
 *   auto v = p.view(0);
 *   v[0] = ...;
 *   p.clear(0);                     // дыра
 *   p.compact();                    // уплотнить
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace slot {

/**
 * Диапазон в пуле: слоты [first, first + count).
 *
 * Лёгкая POD-структура (для uint16_t — 4 байта), копируется по значению.
 * Пустой диапазон: count == 0 (first при clear/resize(0) сбрасывается в 0).
 *
 * @tparam SizeT беззнаковый целочисленный тип индексов (uint16_t по умолчанию)
 */
template <typename SizeT = uint16_t>
struct Range {
    static_assert(std::is_integral_v<SizeT> && std::is_unsigned_v<SizeT>,
                  "Range: SizeT must be unsigned integral");

    using size_type = SizeT;

    SizeT first = 0; /**< индекс первого слота (0, если empty) */
    SizeT count = 0; /**< число слотов; 0 = пусто */

    [[nodiscard]] constexpr bool empty() const noexcept { return count == 0u; }

    /** first + count (конец полуинтервала). */
    [[nodiscard]] constexpr SizeT end() const noexcept
    {
        return static_cast<SizeT>(first + count);
    }
};

static_assert(sizeof(Range<uint16_t>) == 4u);
static_assert(std::is_standard_layout_v<Range<uint16_t>>);
static_assert(std::is_trivially_copyable_v<Range<uint16_t>>);

/**
 * Контракт пула со стёртым типом слота.
 *
 * Публичный API — общая логика раскладки. Тип слота на этом уровне не виден:
 * наследник отдаёт сырой буфер (slotBytes) и размер одного слота в байтах
 * (slotStride). Алгоритмы двигают/обнуляют слоты через memmove/memset.
 *
 * Диапазон адресуется индексом 0 … rangeCount()-1. Индекс вне таблицы
 * в rangeAt() прижимается к 0 (как у sf::SectionPool::at).
 *
 * Копирование запрещено: пул владеет (или оборачивает) единственным
 * буфером слотов.
 *
 * @tparam SizeT тип first/count и ёмкостей (uint16_t по умолчанию)
 */
template <typename SizeT = uint16_t>
class IPool {
public:
    static_assert(std::is_integral_v<SizeT> && std::is_unsigned_v<SizeT>,
                  "IPool: SizeT must be unsigned integral");

    using size_type = SizeT;
    using Range = ::slot::Range<SizeT>;

    virtual ~IPool() = default;

    IPool(const IPool&) = delete;
    IPool& operator=(const IPool&) = delete;

    /** Число слотов в буфере (фиксировано). */
    [[nodiscard]] SizeT slotCapacity() const noexcept { return _slot_capacity; }

    /** Число диапазонов / записей в таблице Range (фиксировано). */
    [[nodiscard]] SizeT rangeCount() const noexcept { return _range_count; }

    /**
     * Bump-указатель: индекс первого байта «хвоста».
     * После compact() равен сумме count живых диапазонов.
     */
    [[nodiscard]] SizeT freeTop() const noexcept { return _free_top; }

    /** slotCapacity() - freeTop() — сколько ещё можно дописать в хвост без compact. */
    [[nodiscard]] SizeT freeSlots() const noexcept
    {
        return static_cast<SizeT>(_slot_capacity - _free_top);
    }

    /**
     * Range с индексом @a idx.
     * idx >= rangeCount() → запись 0 (не UB, но лучше не полагаться).
     */
    [[nodiscard]] Range& rangeAt(SizeT idx) noexcept;
    [[nodiscard]] const Range& rangeAt(SizeT idx) const noexcept;

    /**
     * Выставить число слотов диапазона @a idx в @a n.
     *
     * Поведение:
     *   - n == count           → no-op, true;
     *   - n <  count (shrink)  → обнулить хвост диапазона, count = n
     *                            (n==0 ⇒ first=0); дыру не схлопывает;
     *   - n >  count, count==0 → bump из _free_top, если хватает хвоста;
     *   - n >  count, end==free_top → дописать в хвост на месте;
     *   - n >  count, иначе    → relocate всего диапазона в хвост
     *                            (старое место обнуляется → дыра);
     *   - нехватка хвоста      → false (вызывающий: compact() + повтор,
     *                            либо resizeCompact).
     *
     * @return false если места в хвосте не хватило (или Range битый).
     */
    [[nodiscard]] bool resize(SizeT idx, SizeT n) noexcept;

    /**
     * Освободить диапазон: обнулить слоты, count=0, first=0.
     * Дыру до compact() не схлопывает; free_top не двигает.
     */
    void clear(SizeT idx) noexcept;

    /**
     * Уплотнить пул: живые диапазоны подряд с индекса 0 (порядок по first),
     * обновить first у каждого Range, обнулить хвост, free_top = сумма count.
     *
     * Без scratch-массива order[MaxRanges]: на каждом шаге выбирается живой
     * диапазон с минимальным first среди ещё не сдвинутых (first >= dst).
     * Сложность O(ranges² · move) — ок при малых MaxRanges.
     *
     * Битые Range (first/count вне capacity) обнуляются.
     * Чужой View на следующем доступе перечитает Range.
     */
    void compact() noexcept;

    /**
     * Пересчитать free_top = max(end) по валидным живым Range.
     * Нужен после внешней правки таблицы Range (load, ручной patch).
     */
    void syncFreeTop() noexcept;

    /**
     * Проверка инвариантов: каждый живой Range в пределах capacity,
     * живые диапазоны попарно не пересекаются.
     * Пустые (count==0) не участвуют.
     */
    [[nodiscard]] bool isLayoutOk() const noexcept;

    /**
     * resize(idx, n); при false — compact() и ещё раз resize.
     * Удобная обёртка для роста «с автоуплотнением».
     */
    [[nodiscard]] bool resizeCompact(SizeT idx, SizeT n) noexcept;

protected:
    /**
     * @param slot_capacity ёмкость буфера слотов
     * @param range_count   число записей в таблице Range
     */
    IPool(SizeT slot_capacity, SizeT range_count) noexcept;

    /** Таблица Range[rangeCount] — в RAM, flash-маппинге и т.п. */
    [[nodiscard]] virtual Range* ranges() noexcept = 0;
    [[nodiscard]] virtual const Range* ranges() const noexcept = 0;

    /**
     * Базовый адрес массива слотов как байтовый буфер.
     * Логика пула не знает T — только stride.
     */
    [[nodiscard]] virtual void* slotBytes() noexcept = 0;
    [[nodiscard]] virtual const void* slotBytes() const noexcept = 0;

    /** sizeof(T) у конкретного пула. */
    [[nodiscard]] virtual std::size_t slotStride() const noexcept = 0;

    /** memset 0 для слотов [first, first+count), с clamp по capacity. */
    void zeroSlots(SizeT first, SizeT count) noexcept;

    /** memmove count слотов из src в dst (перекрытие допустимо). */
    void moveSlots(SizeT dst, SizeT src, SizeT count) noexcept;

    SizeT _free_top = 0; /**< bump: следующий свободный индекс слота */

private:
    const SizeT _slot_capacity;
    const SizeT _range_count;
};

/**
 * Вид на один диапазон.
 *
 * Не кэширует first/count между вызовами: перед data/at/size заново читает
 * Range из пула. Поэтому после compact() (в т.ч. чужого диапазона) вид
 * остаётся корректным без ручного rebind.
 *
 * Хранит указатель на базу слотов T* того же пула (типизированный доступ).
 * Пустой/непривязанный вид: operator bool() == false; data()/at → nullptr.
 *
 * @tparam T     тип слота
 * @tparam SizeT тип индексов (должен совпадать с IPool)
 */
template <typename T, typename SizeT = uint16_t>
class View {
public:
    using size_type = SizeT;
    using Range = ::slot::Range<SizeT>;

    View() noexcept = default;

    /**
     * @param pool пул
     * @param idx индекс диапазона
     * @param base  T* = начало массива слотов пула (обычно Pool::data())
     */
    View(IPool<SizeT>& pool, SizeT idx, T* base) noexcept
        : _pool(&pool)
        , _index(idx)
        , _base(base)
    {}

    [[nodiscard]] explicit operator bool() const noexcept { return _pool != nullptr; }

    [[nodiscard]] SizeT index() const noexcept { return _index; }

    /** Текущий Range (копия first/count). */
    [[nodiscard]] Range range() const noexcept
    {
        return _pool != nullptr ? _pool->rangeAt(_index) : Range{};
    }

    [[nodiscard]] SizeT size() const noexcept { return range().count; }

    /**
     * Указатель на первый слот диапазона; nullptr если пусто / нет базы.
     * Адрес пересчитывается из актуального Range.
     */
    [[nodiscard]] T* data() noexcept
    {
        const Range r = range();
        if (_base == nullptr || r.empty()) {
            return nullptr;
        }
        return _base + r.first;
    }

    [[nodiscard]] const T* data() const noexcept
    {
        return const_cast<View*>(this)->data();
    }

    /**
     * Слот i внутри диапазона (0 … size()-1).
     * Вне диапазона / пустой вид → nullptr.
     */
    [[nodiscard]] T* at(SizeT i) noexcept
    {
        const Range r = range();
        if (_base == nullptr || i >= r.count) {
            return nullptr;
        }
        return _base + r.first + i;
    }

    [[nodiscard]] const T* at(SizeT i) const noexcept
    {
        return const_cast<View*>(this)->at(i);
    }

    /**
     * Без проверки на nullptr: вызывающий обязан знать, что i < size().
     * Для безопасного доступа — at().
     */
    [[nodiscard]] T& operator[](SizeT i) noexcept { return *at(i); }
    [[nodiscard]] const T& operator[](SizeT i) const noexcept { return *at(i); }

    /** IPool::resize для своего диапазона. */
    [[nodiscard]] bool resize(SizeT n) noexcept
    {
        return _pool != nullptr && _pool->resize(_index, n);
    }

    /** IPool::clear для своего диапазона. */
    void clear() noexcept
    {
        if (_pool != nullptr) {
            _pool->clear(_index);
        }
    }

private:
    IPool<SizeT>* _pool = nullptr;
    SizeT _index = 0;
    T* _base = nullptr;
};

/**
 * RAM-пул: массив слотов T[Capacity] + таблица Range[MaxRanges].
 *
 * Только хранилище и хуки; вся логика — в IPool.
 * Конструктор обнуляет слоты (memset).
 *
 * @tparam T          тип слота (POD / trivially_copyable)
 * @tparam SizeT      тип индексов
 * @tparam Capacity   число слотов (> 0)
 * @tparam MaxRanges  число диапазонов (> 0)
 *
 * Примеры:
 *   Pool<Profile, uint16_t, 64, 16>
 *   Pool<uint8_t, uint8_t, 32, 4>
 *   Pool16<Profile, 64, 16>          // SizeT = uint16_t
 */
template <typename T, typename SizeT, SizeT Capacity, SizeT MaxRanges>
class Pool : public IPool<SizeT> {
    static_assert(Capacity > 0u, "Pool: Capacity > 0");
    static_assert(MaxRanges > 0u, "Pool: MaxRanges > 0");
    static_assert(!std::is_pointer_v<T>, "Pool: T is the slot type, not T*");
    static_assert(std::is_standard_layout_v<T>, "Pool: T standard_layout");
    static_assert(std::is_trivially_copyable_v<T>, "Pool: T trivially_copyable");

public:
    using size_type = SizeT;
    using Range = ::slot::Range<SizeT>;
    using View = ::slot::View<T, SizeT>;
    using Base = IPool<SizeT>;

    static constexpr SizeT kSlotCapacity = Capacity;
    static constexpr SizeT kRangeCount = MaxRanges;

    Pool() noexcept
        : Base(Capacity, MaxRanges)
    {
        std::memset(_slots, 0, sizeof(_slots));
    }

    /** Вид на диапазон @a idx. */
    [[nodiscard]] View view(SizeT idx) noexcept { return View(*this, idx, _slots); }
    [[nodiscard]] View operator[](SizeT idx) noexcept { return view(idx); }

    /** База массива слотов (для View / прямого доступа по индексу пула). */
    [[nodiscard]] T* data() noexcept { return _slots; }
    [[nodiscard]] const T* data() const noexcept { return _slots; }

protected:
    [[nodiscard]] Range* ranges() noexcept override { return _ranges; }
    [[nodiscard]] const Range* ranges() const noexcept override { return _ranges; }
    [[nodiscard]] void* slotBytes() noexcept override { return _slots; }
    [[nodiscard]] const void* slotBytes() const noexcept override { return _slots; }
    [[nodiscard]] std::size_t slotStride() const noexcept override { return sizeof(T); }

private:
    T _slots[Capacity]{};
    Range _ranges[MaxRanges]{};
};

/**
 * Алиас с SizeT = uint16_t.
 * Pool16&lt;T, Cap, Ranges&gt; ≡ Pool&lt;T, uint16_t, Cap, Ranges&gt;.
 */
template <typename T, uint16_t Capacity, uint16_t MaxRanges>
using Pool16 = Pool<T, uint16_t, Capacity, MaxRanges>;

/* -------------------------------------------------------------------------- */
/* IPool<SizeT> — реализации (header-only шаблон)                        */
/* -------------------------------------------------------------------------- */

template <typename SizeT>
IPool<SizeT>::IPool(SizeT slot_capacity, SizeT range_count) noexcept
    : _slot_capacity(slot_capacity)
    , _range_count(range_count)
{}

template <typename SizeT>
typename IPool<SizeT>::Range& IPool<SizeT>::rangeAt(SizeT idx) noexcept
{
    return ranges()[(idx < _range_count) ? idx : SizeT{0}];
}

template <typename SizeT>
const typename IPool<SizeT>::Range&
IPool<SizeT>::rangeAt(SizeT idx) const noexcept
{
    return ranges()[(idx < _range_count) ? idx : SizeT{0}];
}

template <typename SizeT>
void IPool<SizeT>::zeroSlots(SizeT first, SizeT count) noexcept
{
    if (count == 0u || first >= _slot_capacity) {
        return;
    }
    const SizeT n = (count <= static_cast<SizeT>(_slot_capacity - first))
        ? count
        : static_cast<SizeT>(_slot_capacity - first);
    auto* const p = static_cast<unsigned char*>(slotBytes());
    const std::size_t stride = slotStride();
    std::memset(p + static_cast<std::size_t>(first) * stride, 0,
                static_cast<std::size_t>(n) * stride);
}

template <typename SizeT>
void IPool<SizeT>::moveSlots(SizeT dst, SizeT src, SizeT count) noexcept
{
    if (count == 0u || dst == src) {
        return;
    }
    auto* const p = static_cast<unsigned char*>(slotBytes());
    const std::size_t stride = slotStride();
    std::memmove(p + static_cast<std::size_t>(dst) * stride,
                 p + static_cast<std::size_t>(src) * stride,
                 static_cast<std::size_t>(count) * stride);
}

template <typename SizeT>
bool IPool<SizeT>::resize(SizeT idx, SizeT n) noexcept
{
    Range& pr = rangeAt(idx);
    if (n == pr.count) {
        return true;
    }

    /* shrink: обнулить отброшенный хвост, дыру не трогаем */
    if (n < pr.count) {
        zeroSlots(static_cast<SizeT>(pr.first + n),
                  static_cast<SizeT>(pr.count - n));
        pr.count = n;
        if (n == 0u) {
            pr.first = 0u;
        }
        return true;
    }

    /* ---- grow ---- */

    /* пустой → bump из хвоста */
    if (pr.count == 0u) {
        if (n > static_cast<SizeT>(_slot_capacity - _free_top)) {
            return false;
        }
        pr.first = _free_top;
        pr.count = n;
        zeroSlots(pr.first, n);
        _free_top = static_cast<SizeT>(_free_top + n);
        return true;
    }

    if (pr.first >= _slot_capacity
        || pr.count > static_cast<SizeT>(_slot_capacity - pr.first)) {
        return false; /* битый Range */
    }

    /* диапазон уже в хвосте — дописать на месте */
    if (pr.end() == _free_top) {
        const SizeT need = static_cast<SizeT>(n - pr.count);
        if (need > static_cast<SizeT>(_slot_capacity - _free_top)) {
            return false;
        }
        zeroSlots(static_cast<SizeT>(pr.first + pr.count), need);
        pr.count = n;
        _free_top = static_cast<SizeT>(_free_top + need);
        return true;
    }

    /* relocate в хвост; старое место → дыра */
    if (n > static_cast<SizeT>(_slot_capacity - _free_top)) {
        return false;
    }
    const SizeT src_first = pr.first;
    const SizeT src_count = pr.count;
    const SizeT dst_first = _free_top;
    moveSlots(dst_first, src_first, src_count);
    zeroSlots(static_cast<SizeT>(dst_first + src_count),
              static_cast<SizeT>(n - src_count));
    zeroSlots(src_first, src_count);
    pr.first = dst_first;
    pr.count = n;
    _free_top = static_cast<SizeT>(_free_top + n);
    return true;
}

template <typename SizeT>
void IPool<SizeT>::clear(SizeT idx) noexcept
{
    Range& pr = rangeAt(idx);
    if (pr.count == 0u) {
        pr.first = 0u;
        return;
    }
    if (pr.first < _slot_capacity) {
        const SizeT n = (pr.count <= static_cast<SizeT>(_slot_capacity - pr.first))
            ? pr.count
            : static_cast<SizeT>(_slot_capacity - pr.first);
        zeroSlots(pr.first, n);
    }
    pr.first = 0u;
    pr.count = 0u;
}

template <typename SizeT>
void IPool<SizeT>::compact() noexcept
{
    /* выкинуть битые / нормализовать пустые */
    for (SizeT i = 0u; i < _range_count; ++i) {
        Range& pr = ranges()[i];
        if (pr.count == 0u) {
            pr.first = 0u;
            continue;
        }
        if (pr.first >= _slot_capacity
            || pr.count > static_cast<SizeT>(_slot_capacity - pr.first)) {
            pr.first = 0u;
            pr.count = 0u;
        }
    }

    /*
     * Жадно: пока есть живые с first >= dst, берём с минимальным first,
     * двигаем к dst. Уже уплотнённые имеют first < dst и пропускаются.
     */
    SizeT dst = 0u;
    for (;;) {
        bool found = false;
        SizeT best = 0u;
        SizeT best_first = 0u;
        for (SizeT i = 0u; i < _range_count; ++i) {
            const Range& pr = ranges()[i];
            if (pr.count == 0u || pr.first < dst) {
                continue;
            }
            if (!found || pr.first < best_first) {
                found = true;
                best = i;
                best_first = pr.first;
            }
        }
        if (!found) {
            break;
        }
        Range& pr = ranges()[best];
        if (pr.first != dst) {
            moveSlots(dst, pr.first, pr.count);
            pr.first = dst;
        }
        dst = static_cast<SizeT>(dst + pr.count);
    }

    zeroSlots(dst, static_cast<SizeT>(_slot_capacity - dst));
    _free_top = dst;
}

template <typename SizeT>
void IPool<SizeT>::syncFreeTop() noexcept
{
    SizeT top = 0u;
    for (SizeT i = 0u; i < _range_count; ++i) {
        const Range& pr = ranges()[i];
        if (pr.count == 0u) {
            continue;
        }
        if (pr.first >= _slot_capacity
            || pr.count > static_cast<SizeT>(_slot_capacity - pr.first)) {
            continue;
        }
        const SizeT e = pr.end();
        if (e > top) {
            top = e;
        }
    }
    _free_top = top;
}

template <typename SizeT>
bool IPool<SizeT>::isLayoutOk() const noexcept
{
    for (SizeT i = 0u; i < _range_count; ++i) {
        const Range& a = ranges()[i];
        if (a.count == 0u) {
            continue;
        }
        if (a.first >= _slot_capacity
            || a.count > static_cast<SizeT>(_slot_capacity - a.first)) {
            return false;
        }
        for (SizeT j = static_cast<SizeT>(i + 1u); j < _range_count; ++j) {
            const Range& b = ranges()[j];
            if (b.count == 0u) {
                continue;
            }
            if (b.first >= _slot_capacity
                || b.count > static_cast<SizeT>(_slot_capacity - b.first)) {
                return false;
            }
            if ((a.first < b.end()) && (b.first < a.end())) {
                return false;
            }
        }
    }
    return true;
}

template <typename SizeT>
bool IPool<SizeT>::resizeCompact(SizeT idx, SizeT n) noexcept
{
    if (resize(idx, n)) {
        return true;
    }
    compact();
    return resize(idx, n);
}

} // namespace slot
