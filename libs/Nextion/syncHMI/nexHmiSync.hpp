#pragma once

/**
 * Общие утилиты для сгенерированного nexHmiConfig.hpp (hmi2config.py).
 */

#include "core/nexTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <new>

namespace nex {
class IPage;
} // namespace nex

namespace nex {
namespace hmi {
namespace detail {

[[nodiscard]] inline Literal literalFromPageName(const char* name) noexcept
{
    uint8_t len = 0u;
    while (name[len] != '\0') {
        ++len;
    }
    return Literal{name, len};
}

template<typename T, typename PageMeta>
void emplacePageWidget(IPage& page, void* slot, uint8_t panelId) noexcept
{
    const char* const name = PageMeta::kNames[panelId];
    new (slot) T(page, literalFromPageName(name), panelId);
}

} // namespace detail

// --- X-macros (hmi2config.py: one HMI_PAGE_*(X) list → enum + kNames[]) --------
// X(sym, id): objname == sym (валидный C-идентификатор); строка — HMI_STRINGIFY(sym).

#define HMI_STRINGIFY_(x) #x
#define HMI_STRINGIFY(x) HMI_STRINGIFY_(x)

/** `X(sym, id)` → `sym = id,` in `enum Id : uint8_t`. */
#define HMI_ENUM_ITEM(sym, id, ...) sym = id##u,

/** `X(sym, id)` → NIS objname `"sym"`. */
#define HMI_NAME_ITEM(sym, id, ...) HMI_STRINGIFY(sym),

// --- Component members on nex::Page<> (nexHmiConfig.hpp) ---------------------
// `comp::* (IPage& owner, const Literal& name, uint8_t id)` — owner = *this.
// Имя: HMI_STRINGIFY(sym), не hmi::name() — Literal только из строкового литерала.
// id: PageMeta::sym (тот же panel id, что в enum Id / kNames[]).

/** Panel component id для `sym` на странице `PageMeta`. */
#define HMI_COMP_ID(PageMeta, sym) static_cast<uint8_t>(PageMeta::sym)

/** NIS objname `"sym"` (совпадает с kNames[id], если sym == sanitized objname в HMI). */
#define HMI_COMP_OBJNAME(sym) HMI_STRINGIFY(sym)

/** Три аргумента конструктора comp::*. */
#define HMI_COMP_ARGS(PageMeta, sym) *this, HMI_COMP_OBJNAME(sym), HMI_COMP_ID(PageMeta, sym)

/** `HMI_PAGE_CFG(pgWork)` → `PG`, `InplaceArray` на текущей странице. */
#define HMI_PAGE_CFG(pageSym) \
    using PG = nex::hmi::Page_##pageSym; \
    template<typename T, PG::Id... ids> \
    using InplaceArray = ::nex::hmi::InplacePageArray<T, PG, ids...>

/** Короткая форма на текущей странице; требует `HMI_PAGE_CFG(...)` или `using PG = ...`. */
#define HMI_PAGE_COMP_ARGS(sym) HMI_COMP_ARGS(PG, sym)

/** A: поле + default member init — одна строка в struct ... : Page<N>. Нужен `PG`. */
#define HMI_COMP(Type, sym) Type sym{HMI_PAGE_COMP_ARGS(sym)};

// --- InplacePageArray: плотный массив виджетов (placement new) ----------------
/**
 * Буфер на `sizeof...(Ids)` объектов `T`; placement new в ctor, `~T()` в dtor.
 *
 * @code
 * HMI_PAGE_CFG(browser);
 * using FileRows = InplaceArray<BrowserBtn,
 *     PG::bF0, PG::bF1, PG::bF2, PG::bF3,
 *     PG::bF4, PG::bF5, PG::bF6, PG::bF7>;
 * FileRows fileRows{*this};
 * @endcode
 */
template<typename T, typename PageMeta, typename PageMeta::Id... Ids>
class InplacePageArray {
    static_assert(sizeof...(Ids) > 0u, "InplacePageArray: need at least one id");

    alignas(T) unsigned char mem_[sizeof(T) * sizeof...(Ids)];

    static T* cast_(unsigned char* p) noexcept { return reinterpret_cast<T*>(p); }
    static const T* cast_(const unsigned char* p) noexcept { return reinterpret_cast<const T*>(p); }

    [[nodiscard]] void* slot(uint8_t i) noexcept { return mem_ + sizeof(T) * i; }

    static constexpr uint8_t kIds[sizeof...(Ids)] = { static_cast<uint8_t>(Ids)... };

public:
    using value_type = T;
    static constexpr uint8_t kCount = static_cast<uint8_t>(sizeof...(Ids));
    static constexpr uint8_t capacity = kCount;

    explicit InplacePageArray(IPage& page) noexcept
    {
        uint8_t idx = 0u;
        (detail::emplacePageWidget<T, PageMeta>(page, slot(idx++), static_cast<uint8_t>(Ids)), ...);
    }

    ~InplacePageArray()
    {
        for (uint8_t i = 0u; i < kCount; ++i) {
            (*this)[i].~T();
        }
    }

    InplacePageArray(const InplacePageArray&) = delete;
    InplacePageArray& operator=(const InplacePageArray&) = delete;
    InplacePageArray(InplacePageArray&&) = delete;
    InplacePageArray& operator=(InplacePageArray&&) = delete;

    [[nodiscard]] T& operator[](uint8_t i) noexcept { return *cast_(mem_ + sizeof(T) * i); }
    [[nodiscard]] const T& operator[](uint8_t i) const noexcept
    {
        return *cast_(mem_ + sizeof(T) * i);
    }

    [[nodiscard]] static std::size_t indexOf(uint8_t comp) noexcept
    {
        for (uint8_t i = 0u; i < kCount; ++i) {
            if (kIds[i] == comp) {
                return static_cast<std::size_t>(i);
            }
        }
        return static_cast<std::size_t>(-1);
    }

    [[nodiscard]] static bool contains(uint8_t comp) noexcept
    {
        return indexOf(comp) < static_cast<std::size_t>(kCount);
    }
};

template<typename T, typename PageMeta, typename PageMeta::Id... Ids>
constexpr uint8_t InplacePageArray<T, PageMeta, Ids...>::kIds[sizeof...(Ids)];

} // namespace hmi
} // namespace nex
