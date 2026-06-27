#include "nexComponentBase.hpp"
#include "nexAppUI.hpp"
#include "../core/nexTypes.hpp"

namespace nex {

namespace detail {

void registerPage(IAppUI& app, IPage& page) noexcept {
    const MISC::RegStatus st = app.storage().registerSeq(&page, page.ID);
    if (st != MISC::RegStatus::Ok)
        app.reportRegisterError(st, Route{page.ID, 0u});
}

void registerComponent(IPage& page, Component& c) noexcept {
    MISC::ObjRegistry<Component, uint8_t>& reg = page.storage();

    // `id == 0` в конструкторе — автослот; `registerAt(0, …)` дал бы IdOutOfRange (firstId == kFirstCompId).
    MISC::RegStatus st;
    if (c.id() == 0u) {
        uint8_t assignedId = 0u;
        st = reg.registerAuto(&c, assignedId);
    } else {
        st = reg.registerAt(c.id(), &c);
    }

    if (st != MISC::RegStatus::Ok)
        static_cast<IAppUI&>(page.app).reportRegisterError(st, Route{page.ID, c.id()});
}

} // namespace detail

// --- IPage -----------------------------------------------------------------

IPage::IPage(IAppUI& application, const Literal& pageObjName, uint8_t id) noexcept
    : name(pageObjName), ID(id), app(application) {
    detail::registerPage(application, *this);
}

void IPage::onTouch(const msg::evTouch& e) {
    if (e.route.page != ID)
        return;
    if (e.route.comp == kPageCompId) {
        onTouchPage(e);
        return;
    }
    if (Component* const c = getComponent(e.route.comp)) {
        c->onTouch(e);
    }
}

void IPage::onStatus(const msg::Status& status, Route route) {
    if (Component* const c = getComponent(route.comp))
        c->onStatus(status);
}

void IPage::onResponse(const msg::getNumeric& response, Route route, uint8_t tag) {
    if (Component* const c = getComponent(route.comp))
        c->onResponse(response, tag);
}

void IPage::onResponse(const msg::getString& response, Route route, uint8_t tag) {
    if (Component* const c = getComponent(route.comp))
        c->onResponse(response, tag);
}

uint8_t IPage::compCount() const noexcept {
    return const_cast<IPage*>(this)->storage().registeredCount();
}

Component* IPage::getComponent(uint8_t comp) noexcept {
    if (comp == kPageCompId)
        return nullptr;
    return storage().get(comp);
}

// --- Component ------------------------------------------------------------

Component::Component(IPage& owner, const nex::Literal& compName, Component::Type compType, uint8_t id) noexcept
    : page(owner),
      name(compName),
      type(compType),
      id_(id) {
    detail::registerComponent(owner, *this);
}

void Component::onTouch(const msg::evTouch& e) {
    (void)e;
}

void Component::onResponse(const msg::getNumeric& response, uint8_t tag) {
    (void)response;
    (void)tag;
}

void Component::onResponse(const msg::getString& response, uint8_t tag) {
    (void)response;
    (void)tag;
}

void Component::onStatus(const msg::Status& response) {
    (void)response;
}

} // namespace nex
