#pragma once

/**
 * Общие утилиты для сгенерированного nexHmiConfig.hpp (hmi2config.py).
 */

#include <cstdint>
#include <iterator>
#include <new>

namespace nex {
namespace hmi {

// --- X-macros (hmi2config.py: one HMI_PAGE_*(X) list → enum + kNames[]) --------
// X(sym, id): objname == sym (валидный C-идентификатор); строка — HMI_STRINGIFY(sym).

#define HMI_STRINGIFY_(x) #x
#define HMI_STRINGIFY(x) HMI_STRINGIFY_(x)

/** `X(sym, id)` → `sym = id,` in `enum class Id`. */
#define HMI_ENUM_ITEM(sym, id, ...) sym = id##u,

/** `X(sym, id)` → NIS objname `"sym"`. */
#define HMI_NAME_ITEM(sym, id, ...) HMI_STRINGIFY(sym),

// --- Component members on nex::Page<> (nexHmiConfig.hpp) ---------------------
// `comp::* (IPage& owner, const Literal& name, uint8_t id)` — owner = *this.
// Имя: HMI_STRINGIFY(sym), не hmi::name() — Literal только из строкового литерала.
// id: PageMeta::Id::sym (тот же panel id, что в enum / kNames[]).

/** Panel component id для `sym` на странице `PageMeta`. */
#define HMI_COMP_ID(PageMeta, sym) static_cast<uint8_t>(PageMeta::Id::sym)

/** NIS objname `"sym"` (совпадает с kNames[id], если sym == sanitized objname в HMI). */
#define HMI_COMP_OBJNAME(sym) HMI_STRINGIFY(sym)

/** Три аргумента конструктора comp::*. */
#define HMI_COMP_ARGS(PageMeta, sym) *this, HMI_COMP_OBJNAME(sym), HMI_COMP_ID(PageMeta, sym)

/** `HMI_PAGE_CFG(pgWork)` → `using PageCfg = nex::hmi::Page_pgWork;` (суффикс = objname страницы). */
#define HMI_PAGE_CFG(pageSym) using PageCfg = nex::hmi::Page_##pageSym

/** Короткая форма на текущей странице; требует `HMI_PAGE_CFG(...)` или `using PageCfg = ...`. */
#define HMI_PAGE_COMP_ARGS(sym) HMI_COMP_ARGS(PageCfg, sym)

/** A: поле + default member init — одна строка в struct ... : Page<N>. Нужен `PageCfg`. */
#define HMI_COMP(Type, sym) Type sym{HMI_PAGE_COMP_ARGS(sym)};

/** C: mem-initializer в ctor страницы (когда init list обязателен). */
#define HMI_COMP_INIT(PageMeta, sym) sym(HMI_COMP_ARGS(PageMeta, sym))

/** D: hook для локального X-macro списка `X(Type, sym)` → поле. Нужен PageCfg. */
#define HMI_COMP_X(Type, sym) HMI_COMP(Type, sym)

#define HMI_WIDGET_INIT(sym) HMI_COMP_INIT(PageCfg, sym)

// --- InplaceArray: фиксированный массив без move/default ctor -----------------
/**
 * Буфер на `N` объектов `T` с panel id `IdFirst…IdFirst+N-1`.
 * Placement new в ctor наследника, `~T()` здесь.
 * Для `nex::comp::*` (move удалён, регистрация по адресу на странице).
 *
 * @code
 * HMI_PAGE_CFG(pgWork);
 * HMI_INPLACE_PAGE_RANGE_CFG(PgWorkButtons, pgWork, comp::Button<>, b0, b23)
 * PgWorkButtons b;
 * @endcode
 */
template<typename T, uint8_t N, uint8_t IdFirst = 0u>
class InplaceArray {
    static_assert(N > 0u, "InplaceArray: N must be > 0");

    alignas(T) unsigned char mem_[sizeof(T) * N];

    static T* cast_(unsigned char* p) noexcept { return reinterpret_cast<T*>(p); }
    static const T* cast_(const unsigned char* p) noexcept { return reinterpret_cast<const T*>(p); }

protected:
    InplaceArray() noexcept = default;

    ~InplaceArray()
    {
        for (uint8_t i = 0; i < N; ++i)
            (*this)[i].~T();
    }

    /** Неинициализированный слот `i` (для placement new). */
    [[nodiscard]] void* slot(uint8_t i) noexcept { return mem_ + sizeof(T) * i; }

    /** Слот по panel component id: индекс = `panelId - IdFirst`. */
    [[nodiscard]] void* slot_at(uint8_t panelId) noexcept
    {
        return slot(static_cast<uint8_t>(panelId - IdFirst));
    }

public:
    using value_type = T;
    static constexpr uint8_t kIdFirst = IdFirst;
    static constexpr uint8_t kIdLast = static_cast<uint8_t>(IdFirst + N - 1u);
    static constexpr uint8_t kCount = N;
    static constexpr uint8_t capacity = N;

    InplaceArray(const InplaceArray&) = delete;
    InplaceArray& operator=(const InplaceArray&) = delete;
    InplaceArray(InplaceArray&&) = delete;
    InplaceArray& operator=(InplaceArray&&) = delete;

    [[nodiscard]] T& operator[](uint8_t i) noexcept { return *cast_(mem_ + sizeof(T) * i); }
    [[nodiscard]] const T& operator[](uint8_t i) const noexcept
    {
        return *cast_(mem_ + sizeof(T) * i);
    }
};

/** X-callback для `HMI_PAGE_*(HMI_INPLACE_PAGE_RANGE_ITEM, PageCfg, Type)`. */
#define HMI_INPLACE_PAGE_RANGE_ITEM(sym, id, PageMeta, Type) \
    if ((id) >= kIdFirst && (id) <= kIdLast) \
        new ((this)->slot_at(HMI_COMP_ID(PageMeta, sym))) \
            Type((page), HMI_COMP_OBJNAME(sym), HMI_COMP_ID(PageMeta, sym));

/**
 * Ряд виджетов `firstSym…lastSym` на странице; id из nexHmiConfig.hpp.
 * Перед вызовом: `HMI_PAGE_CFG(pgWork)` → `PageCfg`.
 */
#define HMI_INPLACE_PAGE_RANGE_CFG(Name, pageSym, Type, firstSym, lastSym) \
    struct Name : ::nex::hmi::InplaceArray< \
        Type, \
        static_cast<uint8_t>(HMI_COMP_ID(PageCfg, lastSym) - HMI_COMP_ID(PageCfg, firstSym) + 1u), \
        HMI_COMP_ID(PageCfg, firstSym)> { \
        explicit Name(::nex::IPage& page) noexcept { \
            HMI_PAGE_##pageSym(HMI_INPLACE_PAGE_RANGE_ITEM, PageCfg, Type) \
        } \
    };

// --- lookup -------------------------------------------------------------------
/** Page: `static constexpr const char* kNames[]`, индекс = panel component id. */
template<typename Page, typename T>
[[nodiscard]] static constexpr const char* name(T panelId) noexcept {
    const auto i = static_cast<uint8_t>(panelId);
    return i < std::size(Page::kNames) ? Page::kNames[i] : nullptr;
}

} // namespace hmi
} // namespace nex
