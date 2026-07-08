#include "nexAppUI.hpp"

namespace nex {

IPage* IAppUI::getPage(uint8_t id) noexcept {
    return storage().get(id);
}

uint8_t IAppUI::pageCount() const noexcept {
    return const_cast<IAppUI*>(this)->storage().registeredCount();
}

namespace {

constexpr Literal kKeybdAPage{"keybdA"};
constexpr Literal kKeybdBPage{"keybdB"};
constexpr Literal kKeybdALoadPageId{"keybdA.loadpageid.val"};
constexpr Literal kKeybdALoadCmpId{"keybdA.loadcmpid.val"};
constexpr Literal kKeybdBLoadPageId{"keybdB.loadpageid.val"};
constexpr Literal kKeybdBLoadCmpId{"keybdB.loadcmpid.val"};

void openKeybd(IAppUI& app, const Literal& keybdPageName, const Literal& loadPageId, const Literal& loadCmpId,
               const Component& target) noexcept
{
    app.setGlobalVar(loadPageId, static_cast<int32_t>(target.page.ID));
    app.setGlobalVar(loadCmpId, static_cast<int32_t>(target.id()));
    app.switchPage(keybdPageName);
}

} // namespace

void IAppUI::showKeybdFull(const Component& target) noexcept
{
    openKeybd(*this, kKeybdAPage, kKeybdALoadPageId, kKeybdALoadCmpId, target);
}

void IAppUI::showKeybdNum(const Component& target) noexcept
{
    openKeybd(*this, kKeybdBPage, kKeybdBLoadPageId, kKeybdBLoadCmpId, target);
}

void IAppUI::reportRegisterError(MISC::RegStatus st, Route route) noexcept {
    if (st != MISC::RegStatus::Ok)
        onStatus(appErrorFrom(st), route);
}

void IAppUI::onTouch(const msg::evTouch& e) {
    if (overlay.isModal())
        return;
    if (IPage* const p = getPage(e.route.page))
        p->onTouch(e);
}

void IAppUI::onMsgBox(const msg::evMsgBox& e) noexcept {
    // comp != 0: только виджет; comp == 0: страница (и fallback, если виджет не найден).
    if (e.route.comp != 0u) {
        if (Component* const c = getComponent(e.route)) {
            c->onMsgBox(e);
            return;
        }
    }
    if (IPage* const p = getPage(e.route.page)) {
        p->onMsgBox(e);
        if (e.route.comp == 0u)
            return;
    }
}

void IAppUI::onPageChange(const msg::evPage& e) noexcept {
    const uint8_t prev = currentPage();
    // onExit до обновления `currentPage`, onLoad — после.
    if (prev != e.page) {
        if (IPage* const old_page = getPage(prev))
            old_page->onExit();
    }
    Application::onPageChange(e);
    if (prev != e.page) {
        if (IPage* const new_page = getPage(e.page))
            new_page->onLoad();
    }
}

void IAppUI::onStatus(const msg::Status& status, Route route) noexcept {
    Application::onStatus(status, route);
    if (route.isGlobal())
        return;
    if (IPage* const p = getPage(route.page))
        p->onStatus(status, route);
}

void IAppUI::onResponse(const msg::getNumeric& response, Route route, uint8_t tag) noexcept {
    Application::onResponse(response, route, tag);
    if (IPage* const p = getPage(route.page))
        p->onResponse(response, route, tag);
}

void IAppUI::onResponse(const msg::getString& response, Route route, uint8_t tag) noexcept {
    Application::onResponse(response, route, tag);
    if (IPage* const p = getPage(route.page))
        p->onResponse(response, route, tag);
}

} // namespace nex
