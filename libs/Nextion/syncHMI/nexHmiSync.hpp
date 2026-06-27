#pragma once

/**
 * Общие утилиты для сгенерированного nexHmiConfig.hpp (hmi2config.py).
 */

#include <cstdint>
#include <iterator>

namespace nex {
namespace hmi {

// --- X-macros (hmi2config.py: one HMI_PAGE_*(X) list → enum + kNames[]) --------
// X(sym, id): objname == sym (валидный C-идентификатор); строка — HMI_STRINGIFY(sym).

#define HMI_STRINGIFY_(x) #x
#define HMI_STRINGIFY(x) HMI_STRINGIFY_(x)

/** `X(sym, id)` → `sym = id,` in `enum class Id`. */
#define HMI_ENUM_ITEM(sym, id) sym = id##u,

/** `X(sym, id)` → NIS objname `"sym"`. */
#define HMI_NAME_ITEM(sym, id) HMI_STRINGIFY(sym),

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

/** A: поле + default member init — одна строка в struct ... : Page<N>. */
#define HMI_COMP(Type, PageMeta, sym) Type sym{HMI_COMP_ARGS(PageMeta, sym)};

/** B: как A; перед виджетами — `HMI_PAGE_CFG(pgWork)` или `using PageCfg = ...`. */
#define HMI_WIDGET(Type, sym) HMI_COMP(Type, PageCfg, sym)

/** C: mem-initializer в ctor страницы (когда init list обязателен). */
#define HMI_COMP_INIT(PageMeta, sym) sym(HMI_COMP_ARGS(PageMeta, sym))

/** D: hook для локального X-macro списка `X(Type, sym)` → поле. Нужен PageCfg. */
#define HMI_COMP_X(Type, sym) HMI_COMP(Type, PageCfg, sym)

#define HMI_WIDGET_INIT(sym) HMI_COMP_INIT(PageCfg, sym)

// --- lookup -------------------------------------------------------------------
/** Page: `static constexpr const char* kNames[]`, индекс = panel component id. */
template<typename Page, typename T>
[[nodiscard]] static constexpr const char* name(T panelId) noexcept {
    const auto i = static_cast<uint8_t>(panelId);
    return i < std::size(Page::kNames) ? Page::kNames[i] : nullptr;
}

} // namespace hmi
} // namespace nex
