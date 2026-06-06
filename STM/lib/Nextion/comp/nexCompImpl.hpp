#pragma once

/**
 * Внутренняя иерархия C++-баз для виджетов Nextion.
 * Не подключать из прикладного кода — только через `nexComponents.hpp` / `nexExComponents.hpp` / `nex.hpp`.
 * Конструкторы protected: прямые экземпляры этих классов в HMI не создают.
 */

#include <cstdint>
#include "../core/nexTypes.hpp"
#include "nexAttributes.hpp"
#include "resources/background.hpp"
#include "resources/font.hpp"
#include "resources/pressed.hpp"
#include "nexComponentBase.hpp"

/** Включение атрибутов TouchArea (0 — поле и обработка `onResponse` не компилируются). */
#ifndef NEX_TOUCH_AREA_POSITION
#define NEX_TOUCH_AREA_POSITION 1
#endif
#ifndef NEX_TOUCH_AREA_SIZE
#define NEX_TOUCH_AREA_SIZE 1
#endif

/** Включение атрибутов Drawable (0 — поле и обработка `onResponse` не компилируются). */
#ifndef NEX_DRAWABLE_DRAG
#define NEX_DRAWABLE_DRAG 1
#endif
#ifndef NEX_DRAWABLE_OPACITY
#define NEX_DRAWABLE_OPACITY 1
#endif
#ifndef NEX_DRAWABLE_EFFECT
#define NEX_DRAWABLE_EFFECT 1
#endif

namespace nex {
namespace comp {

/** `pos` (x, y), `w`, `h` (NIS). */
class TouchArea : public Component {
public:
#if NEX_TOUCH_AREA_POSITION
    /** mcu: позиция X */
    attr::Num<uint16_t> x;
    /** mcu: позиция Y */
    attr::Num<uint16_t> y;
#endif
#if NEX_TOUCH_AREA_SIZE
    /** mcu: ширина (RO в ответе get) */
    attr::NumRO<uint16_t> w;
    /** mcu: высота (RO в ответе get) */
    attr::NumRO<uint16_t> h;
#endif

    /** NIS `tsw obj,0|1` — touch switch компонента. */
    void touchSwitch(bool enabled) noexcept;

    /** NIS `click obj,0|1` — программное нажатие/отпускание. */
    void touch(TouchState state = TouchState::Press) noexcept;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override;
    void onResponse(uint8_t tag, const msg::getString& response) override;

protected:
    explicit TouchArea(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Component(owner, objectName, componentType, id)
#if NEX_TOUCH_AREA_POSITION
        , x{*this, attr::Id::X}
        , y{*this, attr::Id::Y}
#endif
#if NEX_TOUCH_AREA_SIZE
        , w{*this, attr::Id::W}
        , h{*this, attr::Id::H}
#endif
    {}
};

/** drag, aph, effect (NIS) */
class Drawable : public TouchArea {
public:
#if NEX_DRAWABLE_DRAG
    void setDraggable(bool enabled) noexcept;
#endif
#if NEX_DRAWABLE_OPACITY
    void setOpacity(uint8_t v) noexcept;
#endif
#if NEX_DRAWABLE_EFFECT
    //TODO сделать enum для effect. Переименовать в setTransition()
    void setTransitionEffect(uint8_t v) noexcept;
#endif

    /** NIS `ref obj` — принудительная перерисовка. */
    void refresh() noexcept;

    /** NIS `vis obj,0|1` — показать/скрыть. */
    void setVisible(bool on) noexcept;
    void show() noexcept;
    void hide() noexcept;

    /** NIS `setlayer obj,above` — разместить над другим виджетом. */
    //TODO сделать метод placeFront - разместить поверх ВСЕХ других виджетов
    void placeAbove(const Drawable& above) noexcept;

    /** NIS `move obj,x0,y0,x1,y1,priority,timeMs` — анимация перемещения. */
    void move(Point from, Point to, uint32_t priority, uint32_t timeMs) noexcept;

protected:
    explicit Drawable(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : TouchArea(owner, objectName, componentType, id)
    {}
};

/**
 * Визуальный компонент с фоном `resources::Background<S>` (`sta` = compile-time `S`).
 */
template<BG S>
class Styled : public Drawable {
public:
    static constexpr BG kStyle = S;

    /** mcu: фон (`bco`/`pic`/`picc` по `S`) — поля внутри `bg` */
    resources::Background<S> bg;

protected:
    explicit Styled(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Drawable(owner, objectName, componentType, id)
        , bg{*this}
    {}
};

/** pco; font; spax — поля в `resources::Font font`. */
template<BG S = BG::Color>
class Printable : public Styled<S> {
public:
    /** mcu: шрифт (`pco`, `font`, `spax`) — поля внутри `font` */
    resources::Font<> font;

protected:
    explicit Printable(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Styled<S>(owner, objectName, componentType, id)
        , font{*this}
    {}
};

/** path; val; hig — `path.maxl` = NIS `path_m`. */
template<BG S = BG::Color>
class ListSelect : public Printable<S> {
public:
    static constexpr uint8_t kCellSizeMin = 16u;
    static constexpr uint8_t kCellSizeMax = 255u;

    /** mcu: путь к списку строк (файл/каталог на панели) */
    attr::String<512> path;
    /** user: выбранный элемент (индекс/значение) */
    attr::Num<int32_t> val;

    /** mcu: высота ячейки списка (NIS `hig`) */
    void setCellSize(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Hig,
            attr_detail::clamp(v, kCellSizeMin, kCellSizeMax));
    }

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (tag == static_cast<uint8_t>(attr::Id::Val)) {
            val.applyResponse(response);
            return;
        }
        TouchArea::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        if (tag == static_cast<uint8_t>(attr::Id::Path)) {
            path.applyResponse(response);
            return;
        }
        TouchArea::onResponse(tag, response);
    }

protected:
    explicit ListSelect(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Printable<S>(owner, objectName, componentType, id)
        , path{*this, attr::Id::Path}
        , val{*this, attr::Id::Val}
    {}
};

/** lineSpacing; wordWrap; vAlign; hAlign */
template<BG S = BG::Color>
class Multiline : public Printable<S> {
public:
    void setLineSpacing(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Spay, v);
    }

    void setWordWrap(bool enabled) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Isbr, enabled);
    }

    // TODO: `ycen` есть не у всех наследников Multiline (SlidingText type 62 — без VAlign в Editor).
    void setVAlign(VAlign v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Ycen, v);
    }

    void setHAlign(HAlign v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Xcen, v);
    }

protected:
    explicit Multiline(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Printable<S>(owner, objectName, componentType, id)
    {}
};

/** NIS `txt`: отправка на панель; зеркало `attr::String<TxtMaxL>` — в листьях. */
template<BG S = BG::Color>
class Textual : public Multiline<S> {
public:
    void setText(const char* text) noexcept
    {
        attr_detail::assignText(*this, attr::Id::Txt, text);
    }

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        TouchArea::onResponse(tag, response);
    }

protected:
    explicit Textual(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Multiline<S>(owner, objectName, componentType, id)
    {}
};

/** bco2/pic2/picc2; pco2 — `resources::Pressed pressed`. */
template<BG S = BG::Color>
class ButtonBase : public Textual<S> {
public:
    /** mcu: оформление нажатого состояния — поля внутри `pressed` */
    resources::Pressed<S> pressed;

protected:
    explicit ButtonBase(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Textual<S>(owner, objectName, componentType, id)
        , pressed{*this}
    {}
};

/** val */
template<BG S = BG::Color>
class Numeric : public Multiline<S> {
public:
    /** user: число (ввод с экранной клавиатуры) */
    attr::Num<int32_t> val;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (tag == static_cast<uint8_t>(attr::Id::Val)) {
            val.applyResponse(response);
            return;
        }
        TouchArea::onResponse(tag, response);
    }

protected:
    explicit Numeric(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Multiline<S>(owner, objectName, componentType, id)
        , val{*this, attr::Id::Val}
    {}
};

/**
 * pco, val; фон — `bco` (Styled). В NIS у Checkbox/Radio нет отдельного `sta`/font;
 * `Style` задаётся шаблоном для единообразия с BG-веткой (по умолчанию Color).
 */

class Selection : public Styled<BG::Color> {
public:
    void setMarkerColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Pco, v);
    }

    /** user: состояние выбора (checkbox/radio/toggle) */
    attr::Num<bool> val;
 
    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (tag == static_cast<uint8_t>(attr::Id::Val)) {
            val.applyResponse(response);
            return;
        }
        TouchArea::onResponse(tag, response);
    }

protected:
    explicit Selection(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Styled<BG::Color>(owner, objectName, componentType, id)
        , val{*this, attr::Id::Val}
    {}
};

/** vid; en; loop; dis; tim; stim(RO); qty(RO) — Gmov, Video. */
class MediaComponent : public Drawable {
public:
    void setVideoId(uint32_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Vid, v);
    }

    void enable() noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::En, true);
    }

    void disable() noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::En, false);
    }

    void setLoop(bool on) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Loop, on);
    }

    void setStep(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Dis, v);
    }

    void setPeriod(uint32_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Tim, v);
    }

    /** user: позиция воспроизведения (RO) */
    attr::NumRO<uint32_t> stim;
    /** user: длительность / количество кадров (RO) */
    attr::NumRO<uint32_t> qty;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case static_cast<uint8_t>(attr::Id::Stim):
            stim.applyResponse(response);
            return;
        case static_cast<uint8_t>(attr::Id::Qty):
            qty.applyResponse(response);
            return;
        default:
            break;
        }
        TouchArea::onResponse(tag, response);
    }

protected:
    explicit MediaComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Drawable(owner, objectName, componentType, id)
        , stim{*this, attr::Id::Stim}
        , qty{*this, attr::Id::Qty}
    {}
};

/**
 * DataFile — поля таблицы/файлов (DataRecord, FileBrowser);
 * `txt`, `qty` — RO в NIS у FileBrowser / DataRecord.
 */
 template<BG S = BG::Color>
 class DataFile : public Printable<S> {
 public:
     /** user: буфер/подпись ячейки (навигация по таблице) */
     attr::String<256> txt;
     /** user: смещение по горизонтали при прокрутке */
     attr::Num<uint16_t> left;
     /** user: индекс канала/строки */
     attr::Num<uint8_t> ch;
     /** user: направление прокрутки */
     attr::Num<uint8_t> dir;
     /** user: значение ячейки / выбор */
     attr::Num<int32_t> val;
     /** user: количество (RO, отражает состояние после действий на панели) */
     attr::NumRO<uint16_t> qty;
     //TODO проверить реальное назначение атрибута dis
     void setCellSpacing(uint16_t v) noexcept
     {
         attr_detail::assignNumeric(*this, attr::Id::Dis, v);
     }
 
     void setMaxvalY(uint16_t v) noexcept
     {
         attr_detail::assignNumeric(*this, attr::Id::MaxvalY, v);
     }
 
     void setMaxvalX(uint16_t v) noexcept
     {
         attr_detail::assignNumeric(*this, attr::Id::MaxvalX, v);
     }
 
     /** user: позиция/значение по X */
     attr::Num<uint16_t> val_x;
     /** user: позиция/значение по Y */
     attr::Num<uint16_t> val_y;
 
     //TODO посмотреть реальное назначение атрибутов bco2 и pco2
     void setBco2(uint16_t v) noexcept
     {
         attr_detail::assignNumeric(*this, attr::Id::Bco2, v);
     }
 
     void setPco2(uint16_t v) noexcept
     {
         attr_detail::assignNumeric(*this, attr::Id::Pco2, v);
     }
 
     void onResponse(uint8_t tag, const msg::getNumeric& response) override
     {
         switch (tag) {
         case static_cast<uint8_t>(attr::Id::Left):
             left.applyResponse(response);
             return;
         case static_cast<uint8_t>(attr::Id::Ch):
             ch.applyResponse(response);
             return;
         case static_cast<uint8_t>(attr::Id::Dir):
             dir.applyResponse(response);
             return;
         case static_cast<uint8_t>(attr::Id::Val):
             val.applyResponse(response);
             return;
         case static_cast<uint8_t>(attr::Id::Qty):
             qty.applyResponse(response);
             return;
         case static_cast<uint8_t>(attr::Id::ValX):
             val_x.applyResponse(response);
             return;
         case static_cast<uint8_t>(attr::Id::ValY):
             val_y.applyResponse(response);
             return;
         default:
             break;
         }
         Printable<S>::onResponse(tag, response);
     }
 
     void onResponse(uint8_t tag, const msg::getString& response) override
     {
         if (tag == static_cast<uint8_t>(attr::Id::Txt)) {
             txt.applyResponse(response);
             return;
         }
         Styled<S>::onResponse(tag, response);
     }
 
 protected:
     explicit DataFile(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
         : Printable<S>(owner, objectName, componentType, id)
         , txt{*this, attr::Id::Txt}
         , left{*this, attr::Id::Left}
         , ch{*this, attr::Id::Ch}
         , dir{*this, attr::Id::Dir}
         , val{*this, attr::Id::Val}
         , qty{*this, attr::Id::Qty}
         , val_x{*this, attr::Id::ValX}
         , val_y{*this, attr::Id::ValY}
     {}
 };

} // namespace comp
} // namespace nex
