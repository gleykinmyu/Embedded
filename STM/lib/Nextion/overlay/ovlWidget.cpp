#include "ovlWidget.hpp"
#include "ovlOverlay.hpp"

#include "../comp/nexCanvas.hpp"

namespace nex::ovl {

void Widget::addChildTop(Object& child) noexcept {
    if (child._parent == this) {
        if (_childTop == &child)
            return;
        removeChild(child);
    } else if (child._parent != nullptr) {
        child._parent->removeChild(child);
    }

    child._parent = this;
    child._above = nullptr;
    child._below = _childTop;
    if (_childTop != nullptr)
        _childTop->_above = &child;
    else
        _childBottom = &child;
    _childTop = &child;
}

void Widget::removeChild(Object& child) noexcept {
    if (child._parent != this)
        return;

    if (_pressedChild == &child)
        _pressedChild = nullptr;

    if (child._above != nullptr)
        child._above->_below = child._below;
    else
        _childTop = child._below;

    if (child._below != nullptr)
        child._below->_above = child._above;
    else
        _childBottom = child._above;

    child._above = nullptr;
    child._below = nullptr;
    child._parent = nullptr;
}

void Widget::layout() noexcept {
    for (Object* n = _childTop; n != nullptr; n = n->below())
        n->layout();
}

void Widget::drawChildren(const AppCanvas& cs) const noexcept {
    if (_childBottom == nullptr)
        return;

    for (const Object* n = _childBottom; n != nullptr; n = n->above()) {
        if (n->isVisible())
            n->draw(cs);
    }
}

void Widget::draw(const AppCanvas& cs) const {
    if (!isVisible())
        return;

    drawBackground(cs);
    drawChildren(cs);
}

Object* Widget::hitTarget(const Point screenPt) noexcept {
    if (!Object::hitTarget(screenPt))
        return nullptr;

    for (Object* n = _childTop; n != nullptr; n = n->below()) {
        if (Object* const hit = n->hitTarget(screenPt))
            return hit;
    }

    return nullptr;
}

bool Widget::onTouchXY(const msg::evTouchXY& e) noexcept {
    if (!isVisible())
        return false;

    if (e.state == TouchState::Press) {
        _pressedChild = nullptr;
        if (Object* const hit = hitTarget(e.pos)) {
            if (hit->onTouchXY(e)) {
                _pressedChild = hit;
                return true;
            }
        }
        return false;
    }

    if (_pressedChild != nullptr) {
        const bool consumed = _pressedChild->onTouchXY(e);
        if (e.state == TouchState::Release) {
            if (_pressedChild->screenRegion().contains(e.pos))
                onClick(_pressedChild);
            _pressedChild = nullptr;
        }
        return consumed;
    }

    return false;
}

void Widget::show(Overlay& ovl, const bool modal) noexcept {
    ovl.showWidget(*this, modal);
}

void Widget::hide(Overlay& ovl) noexcept {
    ovl.hideWidget(*this);
}

void Widget::redraw(const Overlay& ovl) const noexcept {
    ovl.redrawWidget(*this);
}

} // namespace nex::ovl
