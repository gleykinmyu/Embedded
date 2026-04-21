#include "nexComponents.hpp"

namespace nex {

    // --- Page -----------------------------------------------------------------

    Page::Page(uint8_t id) noexcept : ID(id) {}

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

    Component::Component(Page& owner, const char* objectName, Component::Type componentType, uint8_t id) noexcept
        : name(objectName),
          page(owner),
          type(componentType),
          _ID(id) {
        owner.registerComponent(this);
    }

    void Component::onTouch(const msg::TouchCompEvent& e) {
        (void)e;
    }

} // namespace nex
