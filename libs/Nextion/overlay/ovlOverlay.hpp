#pragma once

#include "../core/nexTypes.hpp"
#include "ovlWidget.hpp"

namespace nex {

class Application;

namespace ovl {

/** McUI-слой: стек `Widget` (могут перекрываться), `sendxy` / `tsw`, блок `evTouch` при modal. */
class Overlay {
public:
    explicit Overlay(Application& app) noexcept;

    Application& app;

    /** Показать / скрыть: в списке `_root`, draw, `sendxy` / `tsw`. */
    /** @param modal блок `evTouch` (0x65); одновременно только один modal-виджет. */
    void showWidget(Widget& widget, bool modal = false) noexcept;
    void hideWidget(Widget& widget) noexcept;

    /** Touch: сверху вниз; modal-промах блокирует стек под собой. */
    void dispatchTouchXY(const msg::evTouchXY& e) noexcept;

    /** Перерисовать весь shown-стек снизу вверх (после `hide`). */
    void redrawShownWidgets() const noexcept;

    /** Верхний shown-виджет; `nullptr` — список пуст. */
    [[nodiscard]] Widget* topWidget() const noexcept;
    [[nodiscard]] bool isModal() const noexcept { return _modalWidget != nullptr; }
    [[nodiscard]] static Widget* nextBelow(const Widget* widget) noexcept;
    [[nodiscard]] static Widget* nextAbove(const Widget* widget) noexcept;

private:
    Widget _root;
    Widget* _modalWidget = nullptr;
    bool _sendXY = false;
    bool _touchOff = false;

    void updateInputState() noexcept;

    /** Поднять в стеке; `true` — z-order изменился. */
    bool bringWidgetToFront(Widget& widget) noexcept;

    [[nodiscard]] Widget* bottomWidget() const noexcept;
};

} // namespace ovl
} // namespace nex
