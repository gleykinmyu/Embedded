#pragma once

#include "../core/nexMessages.hpp"
#include "../core/nexTypes.hpp"

namespace nex {

class AppCanvas;

namespace ovl {

class Widget;

/** Лист или ребёнок `Widget`: геометрия в координатах родителя, sibling z-order ( `_above` — выше ).
 *  Дети одного `Widget` не должны пересекаться по `screenRegion()` — partial redraw не поддерживается. */
class Object {
public:
    virtual ~Object() = default;

    /** Локальный `Region` относительно `parent()`; у корня — экранные координаты. */
    void setRegion(const Region region) noexcept { _region = region; }
    [[nodiscard]] const Region& region() const noexcept { return _region; }

    /** Меняет `size`, origin (`ul`) не трогает. */
    virtual void setSize(Rect size) noexcept { setRegion(Region(_region.ul, size)); }

    [[nodiscard]] Widget* parent() const noexcept { return _parent; }

    /** Отвязать от родительского `Widget`; no-op без `parent()`. */
    void remove() noexcept;

    /** Сосед выше по z-order (ближе к `_childTop` родителя). */
    [[nodiscard]] Object* above() const noexcept { return _above; }
    /** Сосед ниже по z-order. */
    [[nodiscard]] Object* below() const noexcept { return _below; }

    [[nodiscard]] virtual bool isVisible() const noexcept { return _visible; }
    void setVisible(const bool visible) noexcept { _visible = visible; }

    /** Пересчёт layout после смены размеров / данных. */
    virtual void layout() noexcept {}

    virtual void draw(const AppCanvas& cs) const;

    /** Top-first hit target; `nullptr` — промах. */
    [[nodiscard]] virtual Object* hitTarget(const Point screenPt) noexcept;

    /** `true` — событие потреблено. */
    virtual bool onTouchXY(const msg::evTouchXY& e) noexcept;

    /** Абсолютный `Region` на экране (с учётом цепочки `parent`). */
    [[nodiscard]] Region screenRegion() const noexcept;

protected:
    Region _region{};
    bool _visible = true;

    Object* _above = nullptr;
    Object* _below = nullptr;
    Widget* _parent = nullptr;

    friend class Widget;
};

} // namespace ovl
} // namespace nex
