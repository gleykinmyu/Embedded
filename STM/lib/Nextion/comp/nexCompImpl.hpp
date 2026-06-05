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
    enum Tag : uint8_t {
        X = 1u,
        Y = 2u,
        W = 3u,
        H = 4u,
    };

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
        , x{*this, "x", Tag::X}
        , y{*this, "y", Tag::Y}
#endif
#if NEX_TOUCH_AREA_SIZE
        , w{*this, "w", Tag::W}
        , h{*this, "h", Tag::H}
#endif
    {}
};

/** drag, aph, effect (NIS) */
class Drawable : public TouchArea {
public:
    enum Tag : uint8_t {
        Drag = 16u,
        Aph = 17u,
        Effect = 18u,
    };

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
template<BGStyle S>
class Styled : public Drawable {
public:
    static constexpr BGStyle kStyle = S;

    /** mcu: фон (`bco`/`pic`/`picc` по `S`) — поля внутри `bg` */
    resources::Background<S> bg;

protected:
    explicit Styled(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Drawable(owner, objectName, componentType, id)
        , bg{*this}
    {}
};

/** pco; font; spax — поля в `resources::Font font`. */
template<BGStyle S = BGStyle::Color>
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
template<BGStyle S = BGStyle::Color>
class ListSelect : public Printable<S> {
public:
    enum Tag : uint8_t {
        Path = 96u,
        Val = 98u,
        Hig = 101u,
    };

    static constexpr uint8_t kCellSizeMin = 16u;
    static constexpr uint8_t kCellSizeMax = 255u;

    /** mcu: путь к списку строк (файл/каталог на панели) */
    attr::String<512> path;
    /** user: выбранный элемент (индекс/значение) */
    attr::Num<int32_t> val;

    /** mcu: высота ячейки списка (NIS `hig`) */
    void setCellSize(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"hig"}, Tag::Hig,
            attr_detail::clamp(v, kCellSizeMin, kCellSizeMax));
    }

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case Tag::Val:
            val.applyResponse(response);
            return;
        default:
            break;
        }
        TouchArea::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        if (tag == Tag::Path) {
            path.applyResponse(response);
            return;
        }
        TouchArea::onResponse(tag, response);
    }

protected:
    explicit ListSelect(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Printable<S>(owner, objectName, componentType, id)
        , path{*this, "path", Tag::Path}
        , val{*this, "val", Tag::Val}
    {}
};

/** lineSpacing; wordWrap; vAlign; hAlign */
template<BGStyle S = BGStyle::Color>
class Multiline : public Printable<S> {
public:
    enum Tag : uint8_t {
        LineSpacing = 112u,
        WordWrap,
        Ycen,
        Xcen,
    };

    void setLineSpacing(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"spay"}, Tag::LineSpacing, v);
    }

    void setWordWrap(bool enabled) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"isbr"}, Tag::WordWrap, enabled);
    }

    void setVAlign(VAlign v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"ycen"}, Tag::Ycen, v);
    }

    void setHAlign(HAlign v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"xcen"}, Tag::Xcen, v);
    }

protected:
    explicit Multiline(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Printable<S>(owner, objectName, componentType, id)
    {}
};

/** txt — `attr::String<TxtMaxL>` (`maxl` = compile-time). */
template<BGStyle S = BGStyle::Color, uint16_t TxtMaxL = 16u>
class TextComponent : public Multiline<S> {
public:
    enum Tag : uint8_t {
        Txt = 128u,
    };

    /** user: текст (ввод с клавиатуры у Text/Number); mcu: подпись у Button */
    attr::String<TxtMaxL> txt;

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        if (tag == Tag::Txt) {
            txt.applyResponse(response);
            return;
        }
        TouchArea::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        TouchArea::onResponse(tag, response);
    }

protected:
    explicit TextComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Multiline<S>(owner, objectName, componentType, id)
        , txt{*this, "txt", Tag::Txt}
    {}
};

/** bco2/pic2/picc2; pco2 — `resources::Pressed pressed`. */
template<BGStyle S = BGStyle::Color, uint16_t TxtMaxL = 16u>
class ButtonLikeComponent : public TextComponent<S, TxtMaxL> {
public:
    /** mcu: оформление нажатого состояния — поля внутри `pressed` */
    resources::Pressed<S> pressed;

protected:
    explicit ButtonLikeComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : TextComponent<S, TxtMaxL>(owner, objectName, componentType, id)
        , pressed{*this}
    {}
};

/** val */
template<BGStyle S = BGStyle::Color>
class Numeric : public Multiline<S> {
public:
    enum Tag : uint8_t {
        Val = 161u,
    };

    /** user: число (ввод с экранной клавиатуры) */
    attr::Num<int32_t> val;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (tag == Tag::Val) {
            val.applyResponse(response);
            return;
        }
        TouchArea::onResponse(tag, response);
    }

protected:
    explicit Numeric(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Multiline<S>(owner, objectName, componentType, id)
        , val{*this, "val", Tag::Val}
    {}
};

/**
 * pco, val; фон — `bco` (Styled). В NIS у Checkbox/Radio нет отдельного `sta`/font;
 * `Style` задаётся шаблоном для единообразия с BG-веткой (по умолчанию Color).
 */

class Selection : public Styled<BGStyle::Color> {
public:
    enum Tag : uint8_t {
        Pco = 48u,
        Val = 177u,
    };

    void setMarkerColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pco"}, Tag::Pco, v);
    }

    /** user: состояние выбора (checkbox/radio/toggle) */
    attr::Num<bool> val;
 
    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (tag == Tag::Val) {
            val.applyResponse(response);
            return;
        }
        TouchArea::onResponse(tag, response);
    }

protected:
    explicit Selection(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Styled<BGStyle::Color>(owner, objectName, componentType, id)
        , val{*this, "val", Tag::Val}
    {}
};

/** vid; en; loop; dis; tim; stim(RO); qty(RO) — Gmov, Video. */
class MediaComponent : public Drawable {
public:
    enum Tag : uint8_t {
        Vid = 24u,
        En,
        Loop,
        Dis,
        Tim,
        Stim,
        Qty,
    };

    void setVideoId(uint32_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"vid"}, Tag::Vid, v);
    }

    void enable() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"en"}, Tag::En, true);
    }

    void disable() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"en"}, Tag::En, false);
    }

    void setLoop(bool on) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"loop"}, Tag::Loop, on);
    }

    void setStep(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"dis"}, Tag::Dis, v);
    }

    void setPeriod(uint32_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"tim"}, Tag::Tim, v);
    }

    /** user: позиция воспроизведения (RO) */
    attr::NumRO<uint32_t> stim;
    /** user: длительность / количество кадров (RO) */
    attr::NumRO<uint32_t> qty;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case Tag::Stim:
            stim.applyResponse(response);
            return;
        case Tag::Qty:
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
        , stim{*this, "stim", Tag::Stim}
        , qty{*this, "qty", Tag::Qty}
    {}
};

/**
 * DataFile — поля таблицы/файлов (DataRecord, FileBrowser);
 * `txt`, `qty` — RO в NIS у FileBrowser / DataRecord.
 */
 template<BGStyle S = BGStyle::Color>
 class DataFile : public Printable<S> {
 public:
     enum Tag : uint8_t {
         Txt = 64u,
         Left = 66u,
         Ch,
         Dir,
         Val,
         Qty,
         Dis,
         MaxvalY,
         MaxvalX,
         ValX,
         ValY,
         Bco2,
         Pco2,
     };
 
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
         attr_detail::assignNumeric(*this, Literal{"dis"}, Tag::Dis, v);
     }
 
     void setMaxvalY(uint16_t v) noexcept
     {
         attr_detail::assignNumeric(*this, Literal{"maxval_y"}, Tag::MaxvalY, v);
     }
 
     void setMaxvalX(uint16_t v) noexcept
     {
         attr_detail::assignNumeric(*this, Literal{"maxval_x"}, Tag::MaxvalX, v);
     }
 
     /** user: позиция/значение по X */
     attr::Num<uint16_t> val_x;
     /** user: позиция/значение по Y */
     attr::Num<uint16_t> val_y;
 
     //TODO посмотреть реальное назначение атрибутов bco2 и pco2
     void setBco2(uint16_t v) noexcept
     {
         attr_detail::assignNumeric(*this, Literal{"bco2"}, Tag::Bco2, v);
     }
 
     void setPco2(uint16_t v) noexcept
     {
         attr_detail::assignNumeric(*this, Literal{"pco2"}, Tag::Pco2, v);
     }
 
     void onResponse(uint8_t tag, const msg::getNumeric& response) override
     {
         switch (tag) {
         case Tag::Left:
             left.applyResponse(response);
             return;
         case Tag::Ch:
             ch.applyResponse(response);
             return;
         case Tag::Dir:
             dir.applyResponse(response);
             return;
         case Tag::Val:
             val.applyResponse(response);
             return;
         case Tag::Qty:
             qty.applyResponse(response);
             return;
         case Tag::ValX:
             val_x.applyResponse(response);
             return;
         case Tag::ValY:
             val_y.applyResponse(response);
             return;
         default:
             break;
         }
         Printable<S>::onResponse(tag, response);
     }
 
     void onResponse(uint8_t tag, const msg::getString& response) override
     {
         if (tag == Tag::Txt) {
             txt.applyResponse(response);
             return;
         }
         Styled<S>::onResponse(tag, response);
     }
 
 protected:
     explicit DataFile(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
         : Printable<S>(owner, objectName, componentType, id)
         , txt{*this, "txt", Tag::Txt}
         , left{*this, "left", Tag::Left}
         , ch{*this, "ch", Tag::Ch}
         , dir{*this, "dir", Tag::Dir}
         , val{*this, "val", Tag::Val}
         , qty{*this, "qty", Tag::Qty}
         , val_x{*this, "val_x", Tag::ValX}
         , val_y{*this, "val_y", Tag::ValY}
     {}
 };

} // namespace comp
} // namespace nex
