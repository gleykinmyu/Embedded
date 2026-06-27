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

    if (_modalWidget != nullptr && !_touchOff) {
        _touchOff = true;
        app.touch.setAllTouchable(false);
    } else if (_modalWidget == nullptr && _touchOff) {
        _touchOff = false;
        app.touch.setAllTouchable(true);
    }
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

bool Overlay::bringWidgetToFront(Widget& widget) noexcept {
    if (widget.parent() != &_root || !widget.isVisible())
        return false;
    if (topWidget() == &widget)
        return false;
    _root.addChildTop(widget);
    return true;
}

void Overlay::dispatchTouchXY(const msg::evTouchXY& e) noexcept {
    for (Widget* w = topWidget(); w != nullptr; w = nextBelow(w)) {
        if (!w->onTouchXY(e)) {
            if (w == _modalWidget)
                return;
            continue;
        }

        const bool raised =
            e.state == TouchState::Press && w->raiseOnPress() && bringWidgetToFront(*w);
        Object* const target = w->takeRedrawTarget();
        if (raised)
            w->draw(app.cs);
        else if (target != nullptr)
            w->redrawObject(*target, app.cs);
        return;
    }
}

} // namespace nex::ovl
