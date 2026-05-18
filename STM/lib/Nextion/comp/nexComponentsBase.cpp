#include "nexComponentBase.hpp"
#include "../app/nexApplication.hpp"

namespace nex {

void nexComponentRegisterFailed(Application& app, Page& page, const Component* c, unsigned maxComponents) noexcept {
    (void)maxComponents;
    const RegisterError code =
        (c == nullptr) ? RegisterError::ComponentNullPointer : RegisterError::ComponentIdOutOfRange;
    const uint8_t comp_id = (c != nullptr) ? c->id() : 0u;
    app.reportRegisterError(Application::Status::ComponentRegisterFailed, code, page.ID, comp_id);
}

void nexComponentRegistryFull(Application& app, Page& page, unsigned maxComponents) noexcept {
    (void)maxComponents;
    app.reportRegisterError(Application::Status::ComponentRegisterFailed, RegisterError::ComponentRegistryFull, page.ID, 0u);
}

// --- PageBase -------------------------------------------------------------
Page::Page(Application& application, const Literal& pageObjName, uint8_t id) noexcept
    : name(pageObjName), ID(id), app(application) {
    application.registerPage(*this);
}

void Page::onTouch(const msg::evTouch& e) {
    if (e.page_id != ID)
        return;
    if (e.component_id == 0u) {
        onTouchPage(e);
        return;
    }
    if (Component* const c = getComponent(e.component_id)) {
        c->onTouch(e);
    }
}

void Page::onError(const msg::Status& status, uint8_t component_id) {
    if (Component* const c = getComponent(component_id))
        c->onError(status);
}

namespace {

unsigned firstFreeRegistrySlot(ComponentRegistryDesc reg) noexcept {
    for (unsigned i = 0u; i < reg.size; ++i) {
        if (reg.slots[i] == nullptr)
            return i;
    }
    return reg.size;
}

} // namespace

void Page::registerComponent(Component* c) noexcept {
    const ComponentRegistryDesc reg = getRegistry();
    if (c == nullptr) {
        nexComponentRegisterFailed(app, *this, nullptr, reg.size);
        return;
    }
    uint8_t id = c->id();
    if (id >= reg.size) {
        nexComponentRegisterFailed(app, *this, c, reg.size);
        return;
    }
    if (id == 0u) {
        const unsigned slot = firstFreeRegistrySlot(reg);
        if (slot >= reg.size) {
            nexComponentRegistryFull(app, *this, reg.size);
            return;
        }
        id = static_cast<uint8_t>(slot);
        c->id_ = id;
    }
    reg.slots[id] = c;
}

SetIdResult Page::rebindComponentId(Component* c, uint8_t newId) noexcept {
    const ComponentRegistryDesc reg = getRegistry();
    if (c == nullptr) {
        nexComponentRegisterFailed(app, *this, nullptr, reg.size);
        return SetIdResult::Failed;
    }
    if (&c->page != this)
        return SetIdResult::Failed;
    if (newId >= reg.size) {
        nexComponentRegisterFailed(app, *this, c, reg.size);
        return SetIdResult::Failed;
    }
    const uint8_t oldId = c->id_;
    if (oldId == newId)
        return SetIdResult::Ok;

    Component* const other = reg.slots[newId];
    if (other == nullptr) {
        if (oldId < reg.size && reg.slots[oldId] == c)
            reg.slots[oldId] = nullptr;
        c->id_ = newId;
        reg.slots[newId] = c;
        return SetIdResult::Ok;
    }
    if (other == c)
        return SetIdResult::Ok;

    if (oldId < reg.size && reg.slots[oldId] == c)
        reg.slots[oldId] = nullptr;

    reg.slots[newId] = c;
    c->id_ = newId;

    reg.slots[oldId] = other;
    other->id_ = oldId;

    return SetIdResult::SwappedWithOccupiedRegistrySlot;
}

Component* Page::getComponent(uint8_t component_id)  noexcept {
    const ComponentRegistryDesc reg = getRegistry();
    if (component_id >= reg.size)
        return nullptr;
    return reg.slots[component_id];
}

// --- Component ------------------------------------------------------------

Component::Component(Page& owner, const nex::Literal& compName, Component::Type compType, uint8_t id) noexcept
    : page(owner),
      name(compName),
      type(compType),
      id_(id) {
    owner.registerComponent(this);
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

SetIdResult Component::setId(uint8_t newId) noexcept {
    return page.rebindComponentId(this, newId);
}

} // namespace nex
