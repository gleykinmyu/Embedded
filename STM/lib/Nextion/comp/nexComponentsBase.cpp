#include "nexComponentBase.hpp"

namespace nex {

// --- Page -----------------------------------------------------------------
Page::Page(const nex::Literal& pageObjName, uint8_t id) noexcept : name(pageObjName), ID(id) {}

void Page::registerComponent(Component* c) noexcept {
    if (c == nullptr || _count >= MAX_COMPONENTS)
        return;
    _registry[_count++] = c;
}

void Page::dispatchTouch(const msg::TouchCompEvent& e) noexcept {
    if (e.page_id != ID)
        return;
    for (unsigned i = 0; i < _count; ++i) {
        Component* const c = _registry[i];
        if (c != nullptr && c->ID() == e.component_id) {
            c->onTouch(e);
            return;
        }
    }
}

// --- Component ------------------------------------------------------------

Component::Component(Page& owner, const nex::Literal& compName, Component::Type compType, uint8_t id) noexcept
    : name(compName),
      page(owner),
      type(compType),
      _ID(id) {
    owner.registerComponent(this);
}

void Component::onTouch(const msg::TouchCompEvent& e) {
    (void)e;
}

} // namespace nex