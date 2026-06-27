#include "ovlObject.hpp"

#include "../comp/nexCanvas.hpp"
#include "ovlWidget.hpp"

namespace nex::ovl {

void Object::draw(const AppCanvas& cs) const {
    (void)cs;
}

void Object::remove() noexcept {
    if (_parent != nullptr)
        _parent->removeChild(*this);
}

Region Object::screenRegion() const noexcept {
    if (_parent == nullptr)
        return _region;
    return Canvas::toScreen(_parent->screenRegion(), _region);
}

Object* Object::hitTarget(const Point screenPt) noexcept {
    if (!isVisible())
        return nullptr;
    if (_region.size.w == 0u || _region.size.h == 0u)
        return nullptr;
    if (!screenRegion().contains(screenPt))
        return nullptr;
    return this;
}

bool Object::onTouchXY(const msg::evTouchXY& e) noexcept {
    (void)e;
    return false;
}

} // namespace nex::ovl
