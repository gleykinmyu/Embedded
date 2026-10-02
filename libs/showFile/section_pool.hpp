/**
 * @file section_pool.hpp
 * @brief IPool / PoolView<Hdr,Slot> + SectionPool<Hdr,Slot,NH,NS>.
 *
 * Вид не знает NH/NS: секции на диске одного формата, ёмкость — у пула в RAM.
 * Слот — PoolView::at (указатель, иначе nullptr). Длина — resize / clearSlots.
 * После load — syncFreeTop(); перед save — compact().
 * IPool::protected — friend PoolView.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "showFile.hpp"

namespace sf {

/** Диапазон в секции-пуле: слоты [first, first+count). */
struct PoolRef {
    uint16_t first = 0;
    uint16_t count = 0;

    [[nodiscard]] constexpr bool isEmpty() const noexcept { return count == 0u; }
    [[nodiscard]] constexpr uint16_t end() const noexcept
    {
        return static_cast<uint16_t>(first + count);
    }
};

static_assert(sizeof(PoolRef) == 4u);
static_assert(std::is_standard_layout_v<PoolRef>);
static_assert(std::is_trivially_copyable_v<PoolRef>);

namespace detail {

template <typename Hdr, typename = void>
struct has_pool_member : std::false_type {};

template <typename Hdr>
struct has_pool_member<Hdr, std::void_t<decltype(std::declval<Hdr&>().pool)>> : std::true_type {
};

template <typename Hdr>
struct pool_member_is_pool_ref
    : std::is_same<std::remove_cv_t<std::remove_reference_t<decltype(std::declval<Hdr&>().pool)>>,
                   PoolRef> {};

} // namespace detail

template <typename Hdr, typename Slot>
class IPool;
template <typename Hdr, typename Slot>
class PoolView;

namespace detail {

template <typename Hdr, typename Slot>
[[nodiscard]] PoolView<Hdr, Slot> view(IPool<Hdr, Slot>& pool, uint16_t i) noexcept;

template <typename Hdr, typename Slot>
[[nodiscard]] IPool<Hdr, Slot>* pool(const PoolView<Hdr, Slot>& view) noexcept;

} // namespace detail

/**
 * Пул без ёмкости в типе. Хранилище — SectionPool<…, NH, NS>.
 */
template <typename Hdr, typename Slot>
class IPool {
public:
    virtual ~IPool() = default;

    IPool(const IPool&) = delete;
    IPool& operator=(const IPool&) = delete;

    const uint16_t headerCount;
    const uint16_t slotCapacity;

    virtual void compact() noexcept = 0;
    virtual void markEdited() noexcept = 0;

protected:
    IPool(uint16_t header_count, uint16_t slot_capacity) noexcept
        : headerCount(header_count)
        , slotCapacity(slot_capacity)
    {}

    [[nodiscard]] virtual Hdr* headerAt(uint16_t i) noexcept = 0;
    [[nodiscard]] virtual Slot* slotBase() noexcept = 0;
    [[nodiscard]] virtual bool resizeOnce(uint16_t i, uint16_t n) noexcept = 0;
    virtual void resetSlots(uint16_t i) noexcept = 0;

    friend class PoolView<Hdr, Slot>;
};

/**
 * Вид на заголовок и его слоты. Тип не зависит от NH/NS.
 * at / size / header перед доступом перечитывают first/count (после compact чужого
 * диапазона вид остаётся живым). resize / clearSlots перепривязывают этот вид.
 */
template <typename Hdr, typename Slot>
class PoolView {
    using Pool = IPool<Hdr, Slot>;
    friend PoolView detail::view<Hdr, Slot>(Pool&, uint16_t) noexcept;
    friend Pool* detail::pool<Hdr, Slot>(const PoolView&) noexcept;

public:
    PoolView() noexcept = default;

    [[nodiscard]] explicit operator bool() const noexcept { return _pool != nullptr; }

    [[nodiscard]] Hdr* header() noexcept
    {
        rebind();
        return _hdr;
    }
    [[nodiscard]] const Hdr* header() const noexcept
    {
        rebind();
        return _hdr;
    }

    [[nodiscard]] uint16_t size() const noexcept
    {
        rebind();
        return _count;
    }
    [[nodiscard]] bool isEmpty() const noexcept { return size() == 0u; }

    [[nodiscard]] Slot* begin() noexcept
    {
        rebind();
        return _slots;
    }
    [[nodiscard]] Slot* end() noexcept
    {
        rebind();
        return _slots + _count;
    }
    [[nodiscard]] const Slot* begin() const noexcept
    {
        rebind();
        return _slots;
    }
    [[nodiscard]] const Slot* end() const noexcept
    {
        rebind();
        return _slots + _count;
    }

    /** Индекс вне size / пустой вид — nullptr. */
    [[nodiscard]] Slot* at(uint16_t i) noexcept
    {
        rebind();
        return (i < _count) ? (_slots + i) : nullptr;
    }
    [[nodiscard]] const Slot* at(uint16_t i) const noexcept
    {
        rebind();
        return (i < _count) ? (_slots + i) : nullptr;
    }

    /**
     * Выставить число слотов. Уменьшение — на месте; рост — bump / перенос;
     * при нехватке — compact и ещё раз. Этот view после успеха актуален.
     * @return false если нет пула / места.
     */
    [[nodiscard]] bool resize(uint16_t n) noexcept
    {
        if (_pool == nullptr) {
            return false;
        }
        if (n > _pool->slotCapacity) {
            return false;
        }
        if (!_pool->resizeOnce(_i, n)) {
            _pool->compact();
            if (!_pool->resizeOnce(_i, n)) {
                return false;
            }
        }
        _pool->markEdited();
        rebind();
        return true;
    }

    /** count=0, first=0; слоты в пуле не двигает (дыра до compact). */
    void clearSlots() noexcept
    {
        if (_pool == nullptr) {
            return;
        }
        _pool->resetSlots(_i);
        rebind();
    }

protected:
    [[nodiscard]] Pool* pool() const noexcept { return _pool; }

    void rebind() const noexcept
    {
        if (_pool == nullptr) {
            _hdr = nullptr;
            _slots = nullptr;
            _count = 0u;
            return;
        }
        _hdr = _pool->headerAt(_i);
        const uint16_t ns = _pool->slotCapacity;
        const PoolRef& pr = _hdr->pool;
        if (pr.count == 0u || pr.first >= ns
            || pr.count > static_cast<uint16_t>(ns - pr.first)) {
            _slots = nullptr;
            _count = 0u;
            return;
        }
        _slots = _pool->slotBase() + pr.first;
        _count = pr.count;
    }

private:
    PoolView(Pool& pool, uint16_t i) noexcept
        : _pool(&pool)
        , _i((i < pool.headerCount) ? i : 0u)
    {
        rebind();
    }

    Pool* _pool = nullptr;
    uint16_t _i = 0;
    mutable Hdr* _hdr = nullptr;
    mutable Slot* _slots = nullptr;
    mutable uint16_t _count = 0;
};

namespace detail {

template <typename Hdr, typename Slot>
PoolView<Hdr, Slot> view(IPool<Hdr, Slot>& pool, uint16_t i) noexcept
{
    return PoolView<Hdr, Slot>(pool, i);
}

template <typename Hdr, typename Slot>
IPool<Hdr, Slot>* pool(const PoolView<Hdr, Slot>& view) noexcept
{
    return view.pool();
}

} // namespace detail

/**
 * Пара секций: заголовки + пул слотов фиксированного размера.
 * Аллокация — bump в хвост (free_top_); дыры лечит compact().
 *
 * @tparam Hdr тип записи секции заголовков (обязан иметь PoolRef pool)
 * @tparam Slot тип записи пула
 * @tparam NH число заголовков
 * @tparam NS ёмкость пула в слотах
 */
template <typename Hdr, typename Slot, uint16_t NH, uint16_t NS>
class SectionPool : public IPool<Hdr, Slot> {
    static_assert(NH > 0u, "SectionPool: NH > 0");
    static_assert(NS > 0u, "SectionPool: NS > 0");
    static_assert(!std::is_pointer_v<Hdr> && !std::is_pointer_v<Slot>,
                  "SectionPool: Hdr/Slot are record types");
    static_assert(std::is_standard_layout_v<Hdr>, "SectionPool: Hdr standard_layout");
    static_assert(std::is_standard_layout_v<Slot>, "SectionPool: Slot standard_layout");
    static_assert(detail::has_pool_member<Hdr>::value, "SectionPool: Hdr must have member `pool`");
    static_assert(detail::pool_member_is_pool_ref<Hdr>::value,
                  "SectionPool: Hdr::pool must be sf::PoolRef");

public:
    static constexpr uint16_t kHeaderCount = NH;
    static constexpr uint16_t kSlotCapacity = NS;

    using View = PoolView<Hdr, Slot>;

    /**
     * Регистрирует две секции в @a file (порядок: heads, затем slots).
     * @a required — на обе: одна без другой не имеет смысла.
     * Ёмкость Show должна вмещать +2 секции.
     */
    SectionPool(IShow& file, uint32_t tag_heads, uint32_t tag_slots, bool required = true) noexcept
        : IPool<Hdr, Slot>(NH, NS)
        , _heads(file, tag_heads, required)
        , _slots(file, tag_slots, required)
    {}

    /** Сколько слотов занято хвостом bump (после sync/compact). */
    [[nodiscard]] uint16_t freeTop() const noexcept { return _free_top; }
    [[nodiscard]] uint16_t freeSlots() const noexcept
    {
        return static_cast<uint16_t>(NS - _free_top);
    }

    /** Пометить шоуфайл edited (правка через PoolView). */
    void markEdited() noexcept override { _heads.markEdited(); }

    /**
     * Вид на заголовок @a i и его слоты.
     * Индекс вне NH → запись 0.
     * count==0 или битый диапазон → header есть, слоты пустые.
     */
    [[nodiscard]] View at(uint16_t i) noexcept { return detail::view(*this, i); }
    [[nodiscard]] View operator[](uint16_t i) noexcept { return at(i); }

    /** Пересчитать free_top_ по max(first+count). После load. */
    void syncFreeTop() const noexcept
    {
        uint16_t top = 0u;
        for (uint16_t i = 0u; i < NH; ++i) {
            const PoolRef& pr = _heads.begin()[i].pool;
            if (pr.count == 0u) {
                continue;
            }
            if (pr.first >= NS || pr.count > static_cast<uint16_t>(NS - pr.first)) {
                continue;
            }
            const uint16_t end = pr.end();
            if (end > top) {
                top = end;
            }
        }
        _free_top = top;
    }

    /**
     * first/count в пределах NS, диапазоны не пересекаются.
     * Пустые (count==0) не участвуют.
     */
    [[nodiscard]] bool isLayoutOk() const noexcept
    {
        for (uint16_t i = 0u; i < NH; ++i) {
            const PoolRef& a = _heads.begin()[i].pool;
            if (a.count == 0u) {
                continue;
            }
            if (a.first >= NS || a.count > static_cast<uint16_t>(NS - a.first)) {
                return false;
            }
            for (uint16_t j = static_cast<uint16_t>(i + 1u); j < NH; ++j) {
                const PoolRef& b = _heads.begin()[j].pool;
                if (b.count == 0u) {
                    continue;
                }
                if (b.first >= NS || b.count > static_cast<uint16_t>(NS - b.first)) {
                    return false;
                }
                const bool overlap = (a.first < b.end()) && (b.first < a.end());
                if (overlap) {
                    return false;
                }
            }
        }
        return true;
    }

    /** Сбросить headers, pool и free_top. */
    void clearData() noexcept
    {
        _heads.clearData();
        _slots.clearData();
        _free_top = 0u;
    }

    /**
     * Уплотнить пул: живые диапазоны подряд с 0, обновить first, free_top = сумма count.
     * Помечает шоуфайл edited. Чужой PoolView на следующем at/size/header перечитается.
     */
    void compact() noexcept override
    {
        uint16_t order[NH]{};
        uint16_t live = 0u;
        for (uint16_t i = 0u; i < NH; ++i) {
            PoolRef& pr = _heads.begin()[i].pool;
            if (pr.count == 0u) {
                pr.first = 0u;
                continue;
            }
            if (pr.first >= NS || pr.count > static_cast<uint16_t>(NS - pr.first)) {
                pr.first = 0u;
                pr.count = 0u;
                continue;
            }
            order[live++] = i;
        }

        for (uint16_t i = 1u; i < live; ++i) {
            const uint16_t key = order[i];
            const uint16_t key_first = _heads.begin()[key].pool.first;
            uint16_t j = i;
            while (j > 0u && _heads.begin()[order[j - 1u]].pool.first > key_first) {
                order[j] = order[j - 1u];
                --j;
            }
            order[j] = key;
        }

        Slot* const base = _slots.begin();
        uint16_t dst = 0u;
        for (uint16_t k = 0u; k < live; ++k) {
            PoolRef& pr = _heads.begin()[order[k]].pool;
            if (pr.first != dst) {
                std::memmove(base + dst, base + pr.first,
                             static_cast<std::size_t>(pr.count) * sizeof(Slot));
                pr.first = dst;
            }
            dst = static_cast<uint16_t>(dst + pr.count);
        }
        for (uint16_t i = dst; i < NS; ++i) {
            base[i] = Slot{};
        }
        _free_top = dst;
        _heads.markEdited();
    }

protected:
    [[nodiscard]] Hdr* headerAt(uint16_t i) noexcept override
    {
        return &_heads.begin()[(i < NH) ? i : 0u];
    }
    [[nodiscard]] Slot* slotBase() noexcept override { return _slots.begin(); }

    [[nodiscard]] bool resizeOnce(uint16_t i, uint16_t n) noexcept override
    {
        PoolRef& pr = _heads.begin()[(i < NH) ? i : 0u].pool;
        if (n == pr.count) {
            return true;
        }

        Slot* const base = _slots.begin();

        if (n < pr.count) {
            for (uint16_t k = n; k < pr.count; ++k) {
                const uint16_t idx = static_cast<uint16_t>(pr.first + k);
                if (idx < NS) {
                    base[idx] = Slot{};
                }
            }
            pr.count = n;
            if (n == 0u) {
                pr.first = 0u;
            }
            return true;
        }

        /* grow */
        if (pr.count == 0u) {
            if (n > static_cast<uint16_t>(NS - _free_top)) {
                return false;
            }
            pr.first = _free_top;
            pr.count = n;
            for (uint16_t k = 0u; k < n; ++k) {
                base[pr.first + k] = Slot{};
            }
            _free_top = static_cast<uint16_t>(_free_top + n);
            return true;
        }

        if (pr.first >= NS || pr.count > static_cast<uint16_t>(NS - pr.first)) {
            return false;
        }

        /* хвост bump — можно дописать */
        if (pr.end() == _free_top) {
            const uint16_t need = static_cast<uint16_t>(n - pr.count);
            if (need > static_cast<uint16_t>(NS - _free_top)) {
                return false;
            }
            for (uint16_t k = pr.count; k < n; ++k) {
                base[pr.first + k] = Slot{};
            }
            pr.count = n;
            _free_top = static_cast<uint16_t>(_free_top + need);
            return true;
        }

        /* новый непрерывный блок в хвосте */
        if (n > static_cast<uint16_t>(NS - _free_top)) {
            return false;
        }
        const uint16_t src_first = pr.first;
        const uint16_t src_count = pr.count;
        const uint16_t dst_first = _free_top;
        for (uint16_t k = 0u; k < src_count; ++k) {
            base[dst_first + k] = base[src_first + k];
        }
        for (uint16_t k = src_count; k < n; ++k) {
            base[dst_first + k] = Slot{};
        }
        for (uint16_t k = 0u; k < src_count; ++k) {
            base[src_first + k] = Slot{};
        }
        pr.first = dst_first;
        pr.count = n;
        _free_top = static_cast<uint16_t>(_free_top + n);
        return true;
    }

    void resetSlots(uint16_t i) noexcept override
    {
        PoolRef& pr = _heads.begin()[(i < NH) ? i : 0u].pool;
        if (pr.count == 0u) {
            pr.first = 0u;
            return;
        }
        Slot* const base = _slots.begin();
        if (pr.first < NS) {
            const uint16_t n = (pr.count <= static_cast<uint16_t>(NS - pr.first))
                ? pr.count
                : static_cast<uint16_t>(NS - pr.first);
            for (uint16_t k = 0u; k < n; ++k) {
                base[pr.first + k] = Slot{};
            }
        }
        pr.first = 0u;
        pr.count = 0u;
        _heads.markEdited();
    }

private:
    Section<Hdr, NH> _heads;
    Section<Slot, NS> _slots;
    mutable uint16_t _free_top = 0;
};

} // namespace sf
