#include "nexCompImpl.hpp"

namespace nex {
namespace comp {

void TouchArea::touchSwitch(bool enabled) noexcept
{
    page.app.enqueue(Transaction{cmd::Component::tsw(name, enabled), page.ID, id()});
}

void TouchArea::touch(TouchState state) noexcept
{
    page.app.enqueue(Transaction{cmd::Component::click(name, state), page.ID, id()});
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
    page.app.enqueue(Transaction{cmd::Component::refresh(name), page.ID, id()});
}

void Drawable::setVisible(bool on) noexcept
{
    page.app.enqueue(Transaction{cmd::Component::visible(name, on), page.ID, id()});
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
    page.app.enqueue(Transaction{
        cmd::Component::setlayer(name, above.name),
        page.ID, id()});
}

void Drawable::move(Point from, Point to, uint32_t priority, uint32_t timeMs) noexcept
{
    page.app.enqueue(Transaction{
        cmd::Move(name, from, to, priority, timeMs),
        page.ID, id()});
}

} // namespace comp
} // namespace nex
