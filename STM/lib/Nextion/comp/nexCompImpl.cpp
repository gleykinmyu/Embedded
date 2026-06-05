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

void TouchArea::onResponse(uint8_t tag, const msg::getNumeric& response)
{
    switch (tag) {
#if NEX_TOUCH_AREA_POSITION
    case TouchArea::Tag::X:
        x.applyResponse(response);
        return;
    case TouchArea::Tag::Y:
        y.applyResponse(response);
        return;
#endif
#if NEX_TOUCH_AREA_SIZE
    case TouchArea::Tag::W:
        w.applyResponse(response);
        return;
    case TouchArea::Tag::H:
        h.applyResponse(response);
        return;
#endif
    default:
        break;
    }
    Component::onResponse(tag, response);
}

void TouchArea::onResponse(uint8_t tag, const msg::getString& response)
{
    Component::onResponse(tag, response);
}

#if NEX_DRAWABLE_DRAG
void Drawable::setDraggable(bool enabled) noexcept
{
    attr_detail::assignNumeric(*this, Literal{"drag"}, Tag::Drag, enabled);
}
#endif

#if NEX_DRAWABLE_OPACITY
void Drawable::setOpacity(uint8_t v) noexcept
{
    attr_detail::assignNumeric(*this, Literal{"aph"}, Tag::Aph, v);
}
#endif

#if NEX_DRAWABLE_EFFECT
void Drawable::setTransitionEffect(uint8_t v) noexcept
{
    attr_detail::assignNumeric(*this, Literal{"effect"}, Tag::Effect, v);
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
