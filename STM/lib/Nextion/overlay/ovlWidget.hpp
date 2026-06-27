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

    /** Style-only: подложка в `obj.screenRegion()` + `obj.draw()`. */
    void redrawObject(const Object& obj, const AppCanvas& cs) const noexcept;

    [[nodiscard]] Object* hitTarget(const Point screenPt) noexcept override;
    bool onTouchXY(const msg::evTouchXY& e) noexcept override;

    /** Снять цель partial-redraw после `onTouchXY` (одноразово). */
    [[nodiscard]] Object* takeRedrawTarget() noexcept;

    /** Показать / скрыть; `modal` — только в `show`. */
    void show(Overlay& ovl, bool modal = false) noexcept;
    void hide(Overlay& ovl) noexcept;

    /** `false` — не поднимать виджет на Press (chrome: dock, backdrop). */
    [[nodiscard]] virtual bool raiseOnPress() const noexcept { return true; }

    /** Верхний ребёнок (максимальный z-order). */
    [[nodiscard]] Object* childTop() const noexcept { return _childTop; }
    /** Нижний ребёнок (минимальный z-order). */
    [[nodiscard]] Object* childBottom() const noexcept { return _childBottom; }

protected:
    virtual void drawBackground(const AppCanvas& cs) const { (void)cs; }

    /** Подложка в `clip` перед перерисовкой дочернего `Object` (default — no-op). */
    virtual void drawBackgroundRegion(const AppCanvas& cs, Region clip) const;

    /** Release внутри `_pressedChild` — клик по `target`. */
    virtual void onClick(Object* target) noexcept { (void)target; }

    /** Press/Release по телу виджета (мимо детей); default — no-op. */
    virtual void onTouch(const msg::evTouchXY& e) noexcept { (void)e; }

private:
    Object* _childTop = nullptr;
    Object* _childBottom = nullptr;
    /** Ребёнок или `this` — тело виджета (`onTouch`). */
    Object* _pressedChild = nullptr;
    Object* _redrawTarget = nullptr;

    [[nodiscard]] Object* hitChildTarget(const Point screenPt) noexcept;

    void drawChildren(const AppCanvas& cs) const noexcept;
};

} // namespace nex::ovl
