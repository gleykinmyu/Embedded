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

void Drawable::refresh() noexcept
{
    page.app.enqueue(Transaction{cmd::Component::refresh(name), page.ID, id()});
}

void Drawable::visible(bool on) noexcept
{
    page.app.enqueue(Transaction{cmd::Component::visible(name, on), page.ID, id()});
}

void Drawable::show() noexcept
{
    visible(true);
}

void Drawable::hide() noexcept
{
    visible(false);
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

void Drawable::onResponse(uint8_t tag, const msg::getNumeric& response)
{
    TouchArea::onResponse(tag, response);
}

void Drawable::onResponse(uint8_t tag, const msg::getString& response)
{
    TouchArea::onResponse(tag, response);
}

} // namespace comp
} // namespace nex
