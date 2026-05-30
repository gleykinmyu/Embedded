#pragma once

/**
 * Внутренняя иерархия C++-баз для виджетов Nextion.
 * Не подключать из прикладного кода — только через `nexComponents.hpp` / `nex.hpp`.
 * Конструкторы protected: прямые экземпляры этих классов в HMI не создают.
 */

#include <cstdint>
#include "../core/nexTypes.hpp"
#include "nexAttributes.hpp"
#include "resources/background.hpp"
#include "resources/font.hpp"
#include "resources/pressed.hpp"
#include "nexComponentBase.hpp"

/** Включение атрибутов GeometryComponent (0 — поле и обработка `onResponse` не компилируются). */
#ifndef NEX_GEOMETRY_POSITION
#define NEX_GEOMETRY_POSITION 1
#endif
#ifndef NEX_GEOMETRY_SIZE
#define NEX_GEOMETRY_SIZE 1
#endif

/** Включение атрибутов VisualComponent (0 — поле и обработка `onResponse` не компилируются). */
#ifndef NEX_VISUAL_DRAG
#define NEX_VISUAL_DRAG 1
#endif
#ifndef NEX_VISUAL_APH
#define NEX_VISUAL_APH 1
#endif
#ifndef NEX_VISUAL_EFFECT
#define NEX_VISUAL_EFFECT 1
#endif

namespace nex {

/** `pos` (x, y), `w`, `h` (NIS). */
class GeometryComponent : public Component {
public:
    enum Tag : uint8_t {
        X = 1u,
        Y = 2u,
        W = 3u,
        H = 4u,
    };

#if NEX_GEOMETRY_POSITION
    attr::Num<uint16_t> x;
    attr::Num<uint16_t> y;
#endif
#if NEX_GEOMETRY_SIZE
    attr::NumRO<uint16_t> w;
    attr::NumRO<uint16_t> h;
#endif

    void onResponse(uint8_t tag, const msg::getNumeric& response) override;
    void onResponse(uint8_t tag, const msg::getString& response) override;

protected:
    explicit GeometryComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Component(owner, objectName, componentType, id)
#if NEX_GEOMETRY_POSITION
        , x{*this, "x", Tag::X}
        , y{*this, "y", Tag::Y}
#endif
#if NEX_GEOMETRY_SIZE
        , w{*this, "w", Tag::W}
        , h{*this, "h", Tag::H}
#endif
    {}
};

/** drag, aph, effect (NIS) */
class VisualComponent : public GeometryComponent {
public:
    enum Tag : uint8_t {
        Drag = 16u,
        Aph = 17u,
        Effect = 18u,
    };

#if NEX_VISUAL_DRAG
    attr::Num<bool> drag;
#endif
#if NEX_VISUAL_APH
    attr::Num<uint8_t> aph;
#endif
#if NEX_VISUAL_EFFECT
    attr::Num<uint8_t> effect;
#endif

    void onResponse(uint8_t tag, const msg::getNumeric& response) override;
    void onResponse(uint8_t tag, const msg::getString& response) override;

protected:
    explicit VisualComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : GeometryComponent(owner, objectName, componentType, id)
#if NEX_VISUAL_DRAG
        , drag{*this, "drag", Tag::Drag}
#endif
#if NEX_VISUAL_APH
        , aph{*this, "aph", Tag::Aph}
#endif
#if NEX_VISUAL_EFFECT
        , effect{*this, "effect", Tag::Effect}
#endif
    {}
};

/**
 * Визуальный компонент с фоном `resources::Background<S>` (`sta` = compile-time `S`).
 */
template<BGStyle S>
class BGComponent : public VisualComponent {
public:
    static constexpr BGStyle kStyle = S;

    static constexpr BGStyle style() noexcept { return S; }

    resources::Background<S> bg;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (bg.onResponse(tag, response))
            return;
        VisualComponent::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        VisualComponent::onResponse(tag, response);
    }

protected:
    explicit BGComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : VisualComponent(owner, objectName, componentType, id)
        , bg{*this}
    {}
};

/** Общая ветка drawable + цветовые каналы (дельта к BG — у листьев). */
template<BGStyle S = BGStyle::Color>
class DrawableColoredComponent : public BGComponent<S> {
public:
    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        BGComponent<S>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        BGComponent<S>::onResponse(tag, response);
    }

protected:
    explicit DrawableColoredComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : BGComponent<S>(owner, objectName, componentType, id)
    {}
};

/** font; pco; spax — поля в `resources::Font font`. */
template<BGStyle S = BGStyle::Color>
class PrintableComponent : public BGComponent<S> {
public:
    resources::Font font;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (font.onResponse(tag, response))
            return;
        BGComponent<S>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        BGComponent<S>::onResponse(tag, response);
    }

protected:
    explicit PrintableComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : BGComponent<S>(owner, objectName, componentType, id)
        , font{*this}
    {}
};

/**
 * DataFileRecordComponent — поля таблицы/файлов (DataRecord, FileBrowser);
 * `txt`, `qty` — RO в NIS у FileBrowser / DataRecord.
 */
template<BGStyle S = BGStyle::Color>
class DataFileRecordComponent : public PrintableComponent<S> {
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

    attr::String<256> txt;
    attr::Num<uint16_t> left;
    attr::Num<uint8_t> ch;
    attr::Num<uint8_t> dir;
    attr::Num<int32_t> val;
    attr::NumRO<uint16_t> qty;
    attr::Num<uint16_t> dis;
    attr::Num<uint16_t> maxval_y;
    attr::Num<uint16_t> maxval_x;
    attr::Num<uint16_t> val_x;
    attr::Num<uint16_t> val_y;
    attr::Num<uint16_t> bco2;
    attr::Num<uint16_t> pco2;

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
        case Tag::Dis:
            dis.applyResponse(response);
            return;
        case Tag::MaxvalY:
            maxval_y.applyResponse(response);
            return;
        case Tag::MaxvalX:
            maxval_x.applyResponse(response);
            return;
        case Tag::ValX:
            val_x.applyResponse(response);
            return;
        case Tag::ValY:
            val_y.applyResponse(response);
            return;
        case Tag::Bco2:
            bco2.applyResponse(response);
            return;
        case Tag::Pco2:
            pco2.applyResponse(response);
            return;
        default:
            break;
        }
        PrintableComponent<S>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        if (tag == Tag::Txt) {
            txt.applyResponse(response);
            return;
        }
        PrintableComponent<S>::onResponse(tag, response);
    }

protected:
    explicit DataFileRecordComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : PrintableComponent<S>(owner, objectName, componentType, id)
        , txt{*this, "txt", Tag::Txt}
        , left{*this, "left", Tag::Left}
        , ch{*this, "ch", Tag::Ch}
        , dir{*this, "dir", Tag::Dir}
        , val{*this, "val", Tag::Val}
        , qty{*this, "qty", Tag::Qty}
        , dis{*this, "dis", Tag::Dis}
        , maxval_y{*this, "maxval_y", Tag::MaxvalY}
        , maxval_x{*this, "maxval_x", Tag::MaxvalX}
        , val_x{*this, "val_x", Tag::ValX}
        , val_y{*this, "val_y", Tag::ValY}
        , bco2{*this, "bco2", Tag::Bco2}
        , pco2{*this, "pco2", Tag::Pco2}
    {}
};

/** path; val; ch; dis; hig — `path.maxl` = NIS `path_m`. */
template<BGStyle S = BGStyle::Color>
class ListSelectTextComponent : public PrintableComponent<S> {
public:
    enum Tag : uint8_t {
        Path = 96u,
        Val = 98u,
        Ch,
        Dis,
        Hig,
    };

    attr::String<512> path;
    attr::Num<int32_t> val;
    attr::Num<uint8_t> ch;
    attr::Num<uint16_t> dis;
    attr::Num<uint16_t> hig;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case Tag::Val:
            val.applyResponse(response);
            return;
        case Tag::Ch:
            ch.applyResponse(response);
            return;
        case Tag::Dis:
            dis.applyResponse(response);
            return;
        case Tag::Hig:
            hig.applyResponse(response);
            return;
        default:
            break;
        }
        PrintableComponent<S>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        if (tag == Tag::Path) {
            path.applyResponse(response);
            return;
        }
        PrintableComponent<S>::onResponse(tag, response);
    }

protected:
    explicit ListSelectTextComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : PrintableComponent<S>(owner, objectName, componentType, id)
        , path{*this, "path", Tag::Path}
        , val{*this, "val", Tag::Val}
        , ch{*this, "ch", Tag::Ch}
        , dis{*this, "dis", Tag::Dis}
        , hig{*this, "hig", Tag::Hig}
    {}
};

/** lineSpacing; wordWrap; vAlign; hAlign */
template<BGStyle S = BGStyle::Color>
class MultilineComponent : public PrintableComponent<S> {
public:
    enum Tag : uint8_t {
        LineSpacing = 112u,
        WordWrap,
        Ycen,
        Xcen,
    };

    attr::Num<uint8_t> lineSpacing;
    attr::Num<bool> wordWrap;
    attr::Num<VAlign> vAlign;
    attr::Num<HAlign> hAlign;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case Tag::LineSpacing:
            lineSpacing.applyResponse(response);
            return;
        case Tag::WordWrap:
            wordWrap.applyResponse(response);
            return;
        case Tag::Ycen:
            vAlign.applyResponse(response);
            return;
        case Tag::Xcen:
            hAlign.applyResponse(response);
            return;
        default:
            break;
        }
        PrintableComponent<S>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        PrintableComponent<S>::onResponse(tag, response);
    }

protected:
    explicit MultilineComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : PrintableComponent<S>(owner, objectName, componentType, id)
        , lineSpacing{*this, "spay", Tag::LineSpacing}
        , wordWrap{*this, "isbr", Tag::WordWrap}
        , vAlign{*this, "ycen", Tag::Ycen}
        , hAlign{*this, "xcen", Tag::Xcen}
    {}
};

/** txt — `attr::String<TxtMaxL>` (`maxl` = compile-time). */
template<BGStyle S = BGStyle::Color, uint16_t TxtMaxL = 256u>
class TextComponent : public MultilineComponent<S> {
public:
    enum Tag : uint8_t {
        Txt = 128u,
    };

    attr::String<TxtMaxL> txt;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        MultilineComponent<S>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        if (tag == Tag::Txt) {
            txt.applyResponse(response);
            return;
        }
        MultilineComponent<S>::onResponse(tag, response);
    }

protected:
    explicit TextComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : MultilineComponent<S>(owner, objectName, componentType, id)
        , txt{*this, "txt", Tag::Txt}
    {}
};

/** bco2/pic2/picc2; pco2 — `resources::Pressed pressed`. */
template<BGStyle S = BGStyle::Color, uint16_t TxtMaxL = 256u>
class ButtonLikeComponent : public TextComponent<S, TxtMaxL> {
public:
    resources::Pressed<S> pressed;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (pressed.onResponse(tag, response))
            return;
        TextComponent<S, TxtMaxL>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        TextComponent<S, TxtMaxL>::onResponse(tag, response);
    }

protected:
    explicit ButtonLikeComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : TextComponent<S, TxtMaxL>(owner, objectName, componentType, id)
        , pressed{*this}
    {}
};

/** val, format — `Keyboard` = compile-time (`key` в NIS). */
template<BGStyle S = BGStyle::Color, BindingKeyboard Keyboard = BindingKeyboard::None>
class NumericComponent : public MultilineComponent<S> {
public:
    static constexpr BindingKeyboard kKeyboard = Keyboard;

    enum Tag : uint8_t {
        Val = 161u,
        Format,
    };

    attr::Num<int32_t> val;
    attr::Num<NumFormat> format;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case Tag::Val:
            val.applyResponse(response);
            return;
        case Tag::Format:
            format.applyResponse(response);
            return;
        default:
            break;
        }
        MultilineComponent<S>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        MultilineComponent<S>::onResponse(tag, response);
    }

protected:
    explicit NumericComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : MultilineComponent<S>(owner, objectName, componentType, id)
        , val{*this, "val", Tag::Val}
        , format{*this, "format", Tag::Format}
    {}
};

/**
 * pco, val; фон — `bco` (BGComponent). В NIS у Checkbox/Radio нет отдельного `sta`/font;
 * `Style` задаётся шаблоном для единообразия с BG-веткой (по умолчанию Color).
 */
template<BGStyle S = BGStyle::Color>
class SelectionComponent : public BGComponent<S> {
public:
    enum Tag : uint8_t {
        Pco = 176u,
        Val,
    };

    attr::Num<nex::Color> color;
    attr::Num<bool> val;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case Tag::Pco:
            color.applyResponse(response);
            return;
        case Tag::Val:
            val.applyResponse(response);
            return;
        default:
            break;
        }
        BGComponent<S>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        BGComponent<S>::onResponse(tag, response);
    }

protected:
    explicit SelectionComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : BGComponent<S>(owner, objectName, componentType, id)
        , color{*this, "pco", Tag::Pco}
        , val{*this, "val", Tag::Val}
    {}
};

} // namespace nex
