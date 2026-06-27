#pragma once

#include <cstdint>

#include "../../Interfaces/obj_registry.hpp"
#include "nexComponentBase.hpp"
#include "../app/nexOvlApp.hpp"

namespace nex {

static constexpr uint8_t kDefaultMaxPages = 16u;

/**
 * `Application` + реестр страниц/компонентов: маршрутизация touch, `get`, `onStatus`, `evMsgBox`.
 * Реестр — protected `storage()`; писатели — ctor `IPage` / `Component` и `detail::register*`.
 */
class IAppUI : public OvlApp {
    friend void detail::registerPage(IAppUI& app, IPage& page) noexcept;
    friend void detail::registerComponent(IPage& page, Component& c) noexcept;

public:
    IAppUI(BIF::IByteStream& stream, Rect screen, AppTiming timing) noexcept
        : OvlApp(stream, screen, timing) {}

    /** Страница из `storage()` по panel `id`; `nullptr` если не зарегистрирована. */
    [[nodiscard]] IPage* getPage(uint8_t id) noexcept;
    /** Число зарегистрированных страниц (ёмкость `AppUI<N>` может быть больше). */
    [[nodiscard]] uint8_t pageCount() const noexcept;

    /** Виджет на странице `route.page`; `nullptr` если страница или `comp` не найдены. */
    [[nodiscard]] Component* getComponent(Route route) noexcept;

    /** Touch → `IPage::onTouch`; игнорируется при modal-виджете в `overlay`. */
    virtual void onTouch(const msg::evTouch& e) override;
    /** `evMsgBox`: сначала компонент (`route.comp`), затем страница. */
    virtual void onMsgBox(const msg::evMsgBox& e) noexcept override;
    /** `evPage`: `onExit`/`onLoad` страниц при смене `currentPage()`. */
    virtual void onPageChange(const msg::evPage& e) noexcept override;
    /** После `Application::onStatus` — `IPage::onStatus` (кроме `route.isGlobal()`). */
    virtual void onStatus(const msg::Status& status, Route route = {}) noexcept override;

    /** Зеркала sysvar в base, затем `IPage::onResponse` → виджет. */
    virtual void onResponse(const msg::getNumeric& response, Route route, uint8_t tag) noexcept override;
    virtual void onResponse(const msg::getString& response, Route route, uint8_t tag) noexcept override;

protected:
    [[nodiscard]] virtual MISC::ObjRegistry<IPage, uint8_t>& storage() noexcept = 0;

    /** `onStatus(AppError::Register, …)` при сбое `ObjRegistry::register`. */
    void reportRegisterError(MISC::RegStatus st, Route route) noexcept;
};

inline Component* IAppUI::getComponent(Route route) noexcept {
    IPage* const p = getPage(route.page);
    if (p == nullptr)
        return nullptr;
    return p->getComponent(route.comp);
}

/** Фиксированная ёмкость реестра страниц (`MaxPages` слотов `IPage*`). */
template <uint8_t MaxPages>
class AppUI : public IAppUI {
    static_assert(MaxPages > 0u, "AppUI: MaxPages must be > 0");

public:
    using IAppUI::IAppUI;

protected:
    [[nodiscard]] MISC::ObjRegistry<IPage, uint8_t>& storage() noexcept override { return _pages; }

private:
    MISC::ObjStorage<IPage, MaxPages, uint8_t, 0> _pages;
};

using DefaultAppUI = AppUI<kDefaultMaxPages>;

} // namespace nex
