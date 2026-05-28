#include "nexComponentBase.hpp"
#include "../app/nexApplication.hpp"
#include "../core/nexTypes.hpp"

namespace nex {

void nexComponentRegisterFailed(Application& app, Page& page, const Component& c, MISC::RegStatus st) noexcept {
    app.reportRegisterError(Application::Status::ComponentRegisterFailed, st, page.ID, c.id());
}

// --- Page -----------------------------------------------------------------

Page::Page(Application& application, const Literal& pageObjName, uint8_t id) noexcept
    : name(pageObjName), ID(id), app(application) {
    application.registerPage(*this);
}

void Page::onTouch(const msg::evTouch& e) {
    if (e.page_id != ID)
        return;
    if (e.comp_id == kPageCompId) {
        onTouchPage(e);
        return;
    }
    if (Component* const c = getComponent(e.comp_id)) {
        c->onTouch(e);
    }
}

void Page::onError(const msg::Status& status, uint8_t comp_id) {
    if (Component* const c = getComponent(comp_id))
        c->onError(status);
}

uint8_t Page::compCount() const noexcept {
    return registry().registeredCount();
}

void Page::registerComponent(Component& c) noexcept {
    MISC::ObjRegistry<Component, uint8_t>& reg = registry();

    // `id == 0` в конструкторе — автослот; `registerAt(0, …)` дал бы IdOutOfRange (firstId == kFirstCompId).
    MISC::RegStatus st;
    if (c.id() == 0u) {
        uint8_t assignedId = 0u;
        st = reg.registerAuto(&c, assignedId);
    } else {
        st = reg.registerAt(c.id(), &c);
    }

    if (st != MISC::RegStatus::Ok)
        nexComponentRegisterFailed(app, *this, c, st);
}

Component* Page::getComponent(uint8_t comp_id) noexcept {
    if (comp_id == kPageCompId)
        return nullptr;
    return registry().get(comp_id);
}

// --- Component ------------------------------------------------------------

Component::Component(Page& owner, const nex::Literal& compName, Component::Type compType, uint8_t id) noexcept
    : page(owner),
      name(compName),
      type(compType),
      id_(id) {
    owner.registerComponent(*this);
}

void Component::onTouch(const msg::evTouch& e) {
    (void)e;
}

void Component::onResponse(uint8_t tag, const msg::getNumeric& response) {
    (void)tag;
    (void)response;
}

void Component::onResponse(uint8_t tag, const msg::getString& response) {
    (void)tag;
    (void)response;
}

void Component::onError(const msg::Status& response) {
    (void)response;
}

} // namespace nex
