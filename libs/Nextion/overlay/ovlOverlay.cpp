#include "ovlOverlay.hpp"

#include "../app/nexApplication.hpp"
#include "../core/nexDebug.hpp"

namespace nex::ovl {

namespace {

const char* touchStateCstr(const TouchState state) noexcept {
    return state == TouchState::Press ? "Press" : "Release";
}

} // namespace

Overlay::Overlay(Application& application) noexcept : app(application) {
    _root.setRegion(Region(Point(0u, 0u), application.screenLayout().size));
}

void Overlay::showWidget(Widget& widget, const bool modal) noexcept {
    Widget* const parent = widget.parent();
    if (parent != nullptr && parent != &_root) {
        NEX_DBG_OVL("[ovl] show SKIP (parent not root) w=%p modal=%u\n", static_cast<void*>(&widget),
            static_cast<unsigned>(modal));
        return;
    }

    if (modal) {
        if (_modalWidget != nullptr && _modalWidget != &widget)
            hideWidget(*_modalWidget);
        _modalWidget = &widget;
    }

    widget.layout();
    widget.setVisible(true);
    _root.addChildTop(widget);
    /* tsw/sendxy до draw: иначе панель перерисует компоненты поверх canvas. */
    updateInputState();
    const Region r = widget.screenRegion();
    NEX_DBG_OVL("[ovl] show w=%p modal=%u region=(%u,%u %ux%u) sendXY=%u touchOff=%u\n",
        static_cast<void*>(&widget), static_cast<unsigned>(modal), static_cast<unsigned>(r.ul.x),
        static_cast<unsigned>(r.ul.y), static_cast<unsigned>(r.size.w), static_cast<unsigned>(r.size.h),
        static_cast<unsigned>(_sendXY), static_cast<unsigned>(_touchOff));
    widget.draw(app.cs);
}

void Overlay::hideWidget(Widget& widget) noexcept {
    if (widget.parent() != &_root) {
        NEX_DBG_OVL("[ovl] hide SKIP (not shown) w=%p\n", static_cast<void*>(&widget));
        return;
    }

    const bool wasModal = (_modalWidget == &widget);
    widget.setVisible(false);
    _root.removeChild(widget);
    if (_modalWidget == &widget)
        _modalWidget = nullptr;
    updateInputState();
    NEX_DBG_OVL("[ovl] hide w=%p wasModal=%u → redrawShown sendXY=%u touchOff=%u\n",
        static_cast<void*>(&widget), static_cast<unsigned>(wasModal), static_cast<unsigned>(_sendXY),
        static_cast<unsigned>(_touchOff));
    redrawShownWidgets();
}

void Overlay::updateInputState() noexcept {
    const bool hasWidgets = topWidget() != nullptr;
    const bool prevSendXY = _sendXY;
    const bool prevTouchOff = _touchOff;

    if (hasWidgets && !_sendXY) {
        _sendXY = true;
        app.touch.sendXY(true);
    } else if (!hasWidgets && _sendXY) {
        _sendXY = false;
        app.touch.sendXY(false);
    }

    if (_modalWidget != nullptr && !_touchOff) {
        _touchOff = true;
        // `rest` / смена страницы могла сбросить sendxy на панели, пока _sendXY в MCU ещё true.
        _sendXY = true;
        app.touch.sendXY(true);
        app.touch.setAllTouchable(false);
    } else if (_modalWidget == nullptr && _touchOff) {
        _touchOff = false;
        app.touch.setAllTouchable(true);
    }

    if (prevSendXY != _sendXY || prevTouchOff != _touchOff) {
        NEX_DBG_OVL("[ovl] input sendXY %u→%u touchOff %u→%u modal=%p has=%u\n",
            static_cast<unsigned>(prevSendXY), static_cast<unsigned>(_sendXY),
            static_cast<unsigned>(prevTouchOff), static_cast<unsigned>(_touchOff),
            static_cast<void*>(_modalWidget), static_cast<unsigned>(hasWidgets));
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
    uint8_t n = 0u;
    for (Widget* w = bottomWidget(); w != nullptr; w = nextAbove(w)) {
        const Region r = w->screenRegion();
        NEX_DBG_OVL("[ovl] redrawShown[%u] w=%p region=(%u,%u %ux%u) vis=%u\n", static_cast<unsigned>(n),
            static_cast<void*>(w), static_cast<unsigned>(r.ul.x), static_cast<unsigned>(r.ul.y),
            static_cast<unsigned>(r.size.w), static_cast<unsigned>(r.size.h),
            static_cast<unsigned>(w->isVisible()));
        w->draw(app.cs);
        ++n;
    }
    if (n == 0u) {
        NEX_DBG_OVL("[ovl] redrawShown (empty)\n");
    }
}

bool Overlay::bringWidgetToFront(Widget& widget) noexcept {
    if (widget.parent() != &_root || !widget.isVisible())
        return false;
    if (topWidget() == &widget)
        return false;
    _root.addChildTop(widget);
    NEX_DBG_OVL("[ovl] raise w=%p\n", static_cast<void*>(&widget));
    return true;
}

void Overlay::dispatchTouchXY(const msg::evTouchXY& e) noexcept {
    NEX_DBG_OVL("[ovl] touchXY %s (%u,%u) modal=%p\n", touchStateCstr(e.state),
        static_cast<unsigned>(e.pos.x), static_cast<unsigned>(e.pos.y), static_cast<void*>(_modalWidget));

    for (Widget* w = topWidget(); w != nullptr; w = nextBelow(w)) {
        if (!w->onTouchXY(e)) {
            /* Modal глотает событие. Release: Press был на HMI до tsw —
               панель отпустит кнопку поверх canvas; поднимаем modal снова. */
            if (w == _modalWidget) {
                if (e.state == TouchState::Release) {
                    NEX_DBG_OVL("[ovl] touchXY modal MISS → full draw w=%p\n", static_cast<void*>(w));
                    w->draw(app.cs);
                } else {
                    NEX_DBG_OVL("[ovl] touchXY modal MISS (no draw) w=%p\n", static_cast<void*>(w));
                }
                return;
            }
            NEX_DBG_OVL("[ovl] touchXY miss w=%p → below\n", static_cast<void*>(w));
            continue;
        }

        const bool raised = e.state == TouchState::Press && w->raiseOnPress() && bringWidgetToFront(*w);
        Object* const target = w->takeRedrawTarget();
        if (raised) {
            NEX_DBG_OVL("[ovl] touchXY HIT raised → full draw w=%p\n", static_cast<void*>(w));
            w->draw(app.cs);
        } else if (target != nullptr) {
            NEX_DBG_OVL("[ovl] touchXY HIT partial target=%p w=%p\n", static_cast<void*>(target),
                static_cast<void*>(w));
            w->redrawObject(*target, app.cs);
        } else {
            NEX_DBG_OVL("[ovl] touchXY HIT no-redraw w=%p\n", static_cast<void*>(w));
        }
        return;
    }
    NEX_DBG_OVL("[ovl] touchXY no widget\n");
}

} // namespace nex::ovl
