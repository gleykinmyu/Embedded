#pragma once

#include "ovlObject.hpp"

namespace nex::ovl {

class Overlay;

/** McUI-виджет: дети-`Object` (без пересечений), draw, touch; top-level — `show` / `hide` через `Overlay`.
 *  Shown-виджеты в стеке `Overlay` могут перекрывать друг друга. */
class Widget : public Object {
public:
    Widget() noexcept { setVisible(false); }

    void addChildTop(Object& child) noexcept;
    void removeChild(Object& child) noexcept;

    void layout() noexcept override;
    void draw(const AppCanvas& cs) const override;

    [[nodiscard]] Object* hitTarget(const Point screenPt) noexcept override;
    bool onTouchXY(const msg::evTouchXY& e) noexcept override;

    /** Показать / скрыть; `modal` — только в `show`. */
    void show(Overlay& ovl, bool modal = false) noexcept;
    void hide(Overlay& ovl) noexcept;

    void redraw(const Overlay& ovl) const noexcept;

    /** Верхний ребёнок (максимальный z-order). */
    [[nodiscard]] Object* childTop() const noexcept { return _childTop; }
    /** Нижний ребёнок (минимальный z-order). */
    [[nodiscard]] Object* childBottom() const noexcept { return _childBottom; }

protected:
    virtual void drawBackground(const AppCanvas& cs) const { (void)cs; }

    /** Release внутри `_pressedChild` — клик по `target`. */
    virtual void onClick(Object* target) noexcept { (void)target; }

private:
    Object* _childTop = nullptr;
    Object* _childBottom = nullptr;
    Object* _pressedChild = nullptr;

    void drawChildren(const AppCanvas& cs) const noexcept;
};

} // namespace nex::ovl
