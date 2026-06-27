#include "ovlOverlay.hpp"

#include "../app/nexApplication.hpp"

namespace nex::ovl {

Overlay::Overlay(Application& application) noexcept : app(application) {
    _root.setRegion(Region(Point(0u, 0u), application.screenLayout().size));
}

void Overlay::showWidget(Widget& widget, const bool modal) noexcept {
    Widget* const parent = widget.parent();
    if (parent != nullptr && parent != &_root)
        return;

    if (modal) {
        if (_modalWidget != nullptr && _modalWidget != &widget)
            hideWidget(*_modalWidget);
        _modalWidget = &widget;
    }

    widget.layout();
    widget.setVisible(true);
    _root.addChildTop(widget);
    widget.draw(app.cs);
    updateInputState();
}

void Overlay::hideWidget(Widget& widget) noexcept {
    if (widget.parent() != &_root)
        return;

    widget.setVisible(false);
    _root.removeChild(widget);
    if (_modalWidget == &widget)
        _modalWidget = nullptr;
    updateInputState();
    redrawShownWidgets();
}

void Overlay::updateInputState() noexcept {
    const bool hasWidgets = topWidget() != nullptr;

    if (hasWidgets && !_sendXY) {
        _sendXY = true;
        app.touch.sendXY(true);
    } else if (!hasWidgets && _sendXY) {
        _sendXY = false;
        app.touch.sendXY(false);
    }

    if (_modalWidget != nullptr)
        app.touch.touchSwitch(false);
}

Widget* Overlay::topWidget() const noexcept {
    return static_cast<Widget*>(_root.childTop());
}

Widget* Overlay::nextBelow(const Widget* widget) noexcept {
    if (widget == nullptr)
        return nullptr;
    return static_cast<Widget*>(widget->below());
}

Widget* Overlay::nextAbove(const Widget* widget) noexcept {
    if (widget == nullptr)
        return nullptr;
    return static_cast<Widget*>(widget->above());
}

Widget* Overlay::bottomWidget() const noexcept {
    return static_cast<Widget*>(_root.childBottom());
}

void Overlay::redrawShownWidgets() const noexcept {
    for (Widget* w = bottomWidget(); w != nullptr; w = nextAbove(w))
        w->draw(app.cs);
}

void Overlay::redrawWidget(const Widget& widget) const noexcept {
    if (widget.parent() != &_root || !widget.isVisible())
        return;
    const Region base = widget.screenRegion();
    for (const Widget* w = &widget; w != nullptr; w = nextAbove(w)) {
        if (w == &widget || base.overlaps(w->screenRegion()))
            w->draw(app.cs);
    }
}

void Overlay::dispatchTouchXY(const msg::evTouchXY& e) const noexcept {
    for (Widget* w = topWidget(); w != nullptr; w = nextBelow(w)) {
        if (w->onTouchXY(e)) {
            redrawWidget(*w);
            return;
        }
        if (w == _modalWidget)
            return;
    }
}

} // namespace nex::ovl
