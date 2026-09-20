#include "nexCompImpl.hpp"

namespace nex {
namespace comp {

namespace {

void enqueueComp(Component& c, const Command& cmd) noexcept
{
    if (!c.canAccess()) {
        return;
    }
    c.page.app.enqueue(Transaction{cmd, c.page.ID, c.id()});
}

} // namespace

void TouchArea::setTouchable(bool on) noexcept
{
    enqueueComp(*this, cmd::Component::tsw(attr_detail::makeCompRef(*this), on));
}

void TouchArea::touch(TouchState state) noexcept
{
    enqueueComp(*this, cmd::Component::click(attr_detail::makeCompRef(*this), state));
}

void TouchArea::onResponse(const msg::getNumeric& response, uint8_t tag)
{
    switch (tag) {
#if NEX_TOUCH_AREA_POSITION
    case static_cast<uint8_t>(attr::Id::X):
        x.applyResponse(response);
        return;
    case static_cast<uint8_t>(attr::Id::Y):
        y.applyResponse(response);
        return;
#endif
#if NEX_TOUCH_AREA_SIZE
    case static_cast<uint8_t>(attr::Id::W):
        w.applyResponse(response);
        return;
    case static_cast<uint8_t>(attr::Id::H):
        h.applyResponse(response);
        return;
#endif
    default:
        break;
    }
    Component::onResponse(response, tag);
}

void TouchArea::onResponse(const msg::getString& response, uint8_t tag)
{
    Component::onResponse(response, tag);
}

#if NEX_DRAWABLE_DRAG
void Drawable::setDraggable(bool enabled) noexcept
{
    attr_detail::assignNumeric(*this, attr::Id::Drag, enabled);
}
#endif

#if NEX_DRAWABLE_OPACITY
void Drawable::setOpacity(uint8_t v) noexcept
{
    attr_detail::assignNumeric(*this, attr::Id::Aph, v);
}
#endif

#if NEX_DRAWABLE_EFFECT
void Drawable::setTransitionEffect(uint8_t v) noexcept
{
    attr_detail::assignNumeric(*this, attr::Id::Effect, v);
}
#endif

void Drawable::refresh() noexcept
{
    enqueueComp(*this, cmd::Component::refresh(attr_detail::makeCompRef(*this)));
}

void Drawable::setVisible(bool on) noexcept
{
    enqueueComp(*this, cmd::Component::visible(attr_detail::makeCompRef(*this), on));
}

void Drawable::show() noexcept
{
    setVisible(true);
}

void Drawable::hide() noexcept
{
    setVisible(false);
}

void Drawable::placeAbove(const Drawable& above) noexcept
{
    if (&page != &above.page) {
        return;
    }
    enqueueComp(*this, cmd::Component::setlayer(attr_detail::makeCompRef(*this), above.name));
}

void Drawable::move(Point from, Point to, uint32_t priority, uint32_t timeMs) noexcept
{
    enqueueComp(*this, cmd::Move(attr_detail::makeCompRef(*this), from, to, priority, timeMs));
}

} // namespace comp
} // namespace nex
