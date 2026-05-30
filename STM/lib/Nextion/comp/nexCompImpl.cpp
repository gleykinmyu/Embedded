#include "nexCompImpl.hpp"

namespace nex {

void GeometryComponent::onResponse(uint8_t tag, const msg::getNumeric& response)
{
    switch (tag) {
#if NEX_GEOMETRY_POSITION
    case GeometryComponent::Tag::X:
        x.applyResponse(response);
        return;
    case GeometryComponent::Tag::Y:
        y.applyResponse(response);
        return;
#endif
#if NEX_GEOMETRY_SIZE
    case GeometryComponent::Tag::W:
        w.applyResponse(response);
        return;
    case GeometryComponent::Tag::H:
        h.applyResponse(response);
        return;
#endif
    default:
        break;
    }
    Component::onResponse(tag, response);
}

void GeometryComponent::onResponse(uint8_t tag, const msg::getString& response)
{
    Component::onResponse(tag, response);
}

void VisualComponent::onResponse(uint8_t tag, const msg::getNumeric& response)
{
    switch (tag) {
#if NEX_VISUAL_DRAG
    case VisualComponent::Tag::Drag:
        drag.applyResponse(response);
        return;
#endif
#if NEX_VISUAL_APH
    case VisualComponent::Tag::Aph:
        aph.applyResponse(response);
        return;
#endif
#if NEX_VISUAL_EFFECT
    case VisualComponent::Tag::Effect:
        effect.applyResponse(response);
        return;
#endif
    default:
        break;
    }
    GeometryComponent::onResponse(tag, response);
}

void VisualComponent::onResponse(uint8_t tag, const msg::getString& response)
{
    GeometryComponent::onResponse(tag, response);
}

} // namespace nex
