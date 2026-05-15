#include "nexComponentBase.hpp"
#include "nexApplication.hpp"

namespace nex {

void nexComponentRegisterFailed(Application& app, PageBase& page, const Component* c, unsigned maxComponents) noexcept {
    const Literal& pageName = page.name;
    const unsigned pageId = static_cast<unsigned>(page.ID);
    if (c == nullptr) {
        app.Error("nex: registerComponent: null pointer page_id=%u name=%.*s\n", pageId, static_cast<int>(pageName.len),
            pageName.data);
        return;
    }
    const unsigned compId = static_cast<unsigned>(c->ID());
    const unsigned compType = static_cast<unsigned>(c->type);
    app.Error(
        "nex: registerComponent: component id=%u out of range (need < %u) type=%u page_id=%u name=%.*s\n", compId,
        maxComponents, compType, pageId, static_cast<int>(pageName.len), pageName.data);
}

// --- PageBase -------------------------------------------------------------
PageBase::PageBase(Application& application, const Literal& pageObjName, uint8_t id) noexcept
    : name(pageObjName), ID(id), application(application) {
    application.registerPage(*this);
}

void PageBase::dispatchTouch(const msg::TouchEvent& e) noexcept {
    if (e.page_id != ID)
        return;
    onTouch(e);
    if (Component* const c = getComponent(e.component_id))
        c->onTouch(e);
}

void PageBase::onTouch(const msg::TouchEvent& e) {
    (void)e;
}

void PageBase::onAppEvent(const Message& m) {
    (void)m;
}

// --- Component ------------------------------------------------------------

Component::Component(PageBase& owner, const nex::Literal& compName, Component::Type compType, uint8_t id) noexcept
    : application(owner.application),
      name(compName),
      type(compType),
      _ID(id) {
    owner.registerComponent(this);
}

Component::Component(Application& app, const nex::Literal& compName, Component::Type compType, uint8_t id) noexcept
    : application(app),
      name(compName),
      type(compType),
      _ID(id) {}

void Component::onTouch(const msg::TouchEvent& e) {
    (void)e;
}

} // namespace nex
