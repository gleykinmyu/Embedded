#pragma once

#include <cstdint>
#include "../core/nexTypes.hpp"
#include "nexComponentBase.hpp"
#include "nexCompImpl.hpp"

namespace nex {
namespace comp {
// --- Прямые наследники Component (без x,y,w,h на странице) --------------------

/** tim, en */
class Timer : public Component {
public:
    enum Tag : uint8_t {
        Tim = 192u,
        En,
    };

    void setTim(uint32_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"tim"}, Tag::Tim, v);
    }

    void enable() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"en"}, Tag::En, true);
    }

    void disable() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"en"}, Tag::En, false);
    }

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        Component::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        Component::onResponse(tag, response);
    }

    Timer(Page& owner, const Literal& name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Timer, id)
    {}
};

/** NIS `va` при sta=Number: `val`. Тот же `type`=52, что у StringVariable. */
class NumericVariable : public Component {
public:
    enum Tag : uint8_t {
        Val = 192u,
    };

    /** user: значение глобальной числовой переменной */
    attr::Num<int32_t> val;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (tag == Tag::Val) {
            val.applyResponse(response);
            return;
        }
        Component::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        Component::onResponse(tag, response);
    }

    NumericVariable(Page& owner, const Literal& name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Variable, id)
        , val{*this, "val", Tag::Val}
    {}
};

/** NIS `va` при sta=String: `txt`. Тот же `type`=52, что у NumericVariable. */
template<uint16_t TxtMaxL = 256u>
class StringVariable : public Component {
public:
    enum Tag : uint8_t {
        Txt = 192u,
    };

    /** user: значение глобальной строковой переменной */
    attr::String<TxtMaxL> txt;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        Component::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        if (tag == Tag::Txt) {
            txt.applyResponse(response);
            return;
        }
        Component::onResponse(tag, response);
    }

    StringVariable(Page& owner, const Literal& name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Variable, id)
        , txt{*this, "txt", Tag::Txt}
    {}
};

// --- Геометрия и визуальная оболочка (листья; базы — nexCompImpl.hpp) --------

class Hotspot : public TouchArea {
public:
    Hotspot(Page& owner, const Literal& name, uint8_t id = 0)
        : TouchArea(owner, name, Component::Type::Hotspot, id) {}
};

/** pco; dis; txt (данные QR) */
class QRCode : public Styled<BGStyle::Color> {
public:
    enum Tag : uint8_t {
        Pco = 192u,
        Dis,
        Txt,
    };

    void setPco(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pco"}, Tag::Pco, v);
    }

    void setDis(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"dis"}, Tag::Dis, v);
    }

    /** mcu: полезная нагрузка QR (строка) */
    attr::String<256> txt;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        Styled<BGStyle::Color>::onResponse(tag, response);
        (void)tag;
        (void)response;
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        if (tag == Tag::Txt) {
            txt.applyResponse(response);
            return;
        }
        Styled<BGStyle::Color>::onResponse(tag, response);
    }

    QRCode(Page& owner, const Literal& name, uint8_t id = 0)
        : Styled<BGStyle::Color>(owner, name, Component::Type::QRCode, id)
        , txt{*this, "txt", Tag::Txt}
    {}
};

class Picture : public Styled<BGStyle::Image> {
public:
    Picture(Page& owner, const Literal& name, uint8_t id = 0)
        : Styled<BGStyle::Image>(owner, name, Component::Type::Picture, id) {}
};

class CropPicture : public Styled<BGStyle::CropImage> {
public:
    enum Tag : uint8_t {
        Cpic = 192u,
    };

    void setCrop(PicId v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"cpic"}, Tag::Cpic, v);
    }

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        Styled<BGStyle::CropImage>::onResponse(tag, response);
        (void)tag;
        (void)response;
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        Styled<BGStyle::CropImage>::onResponse(tag, response);
    }

    CropPicture(Page& owner, const Literal& name, uint8_t id = 0)
        : Styled<BGStyle::CropImage>(owner, name, Component::Type::CropPicture, id)
    {}
};

template<BGStyle S = BGStyle::Color>
class Waveform : public Styled<S> {
public:
    enum Tag : uint8_t {
        Ch = 192u,
        Pco0,
        Pco1,
        Pco2,
        Pco3,
        Gdc,
        Gdw,
        Gdh,
        Dis,
        Wid,
        Hig,
    };

    void setCh(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"ch"}, Tag::Ch, v);
    }

    void setPco0(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pco0"}, Tag::Pco0, v);
    }

    void setPco1(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pco1"}, Tag::Pco1, v);
    }

    void setPco2(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pco2"}, Tag::Pco2, v);
    }

    void setPco3(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pco3"}, Tag::Pco3, v);
    }

    void setGdc(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"gdc"}, Tag::Gdc, v);
    }

    void setGdw(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"gdw"}, Tag::Gdw, v);
    }

    void setGdh(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"gdh"}, Tag::Gdh, v);
    }

    void setDis(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"dis"}, Tag::Dis, v);
    }

    void setWid(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"wid"}, Tag::Wid, v);
    }

    void setHig(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"hig"}, Tag::Hig, v);
    }

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        Styled<S>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        Styled<S>::onResponse(tag, response);
    }

    Waveform(Page& owner, const Literal& name, uint8_t id = 0)
        : Styled<S>(owner, name, Component::Type::Waveform, id)
    {}
};

template<BGStyle S = BGStyle::Color>
class ProgressBar {
    static_assert(S == BGStyle::Color || S == BGStyle::Image,
        "ProgressBar: only BGStyle::Color and BGStyle::Image are supported");
};

/** sta=color: barColor (pco); cornerRadius (dis); фон — Styled::bg */
template<>
class ProgressBar<BGStyle::Color> : public Linear<BGStyle::Color> {
public:
    enum Tag : uint8_t {
        BarColor = 193u,
        CornerRadius,
    };

    void setBarColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pco"}, Tag::BarColor, v);
    }

    void setCornerRadius(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"dis"}, Tag::CornerRadius, v);
    }

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        Linear<BGStyle::Color>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        Linear<BGStyle::Color>::onResponse(tag, response);
    }

    ProgressBar(Page& owner, const Literal& name, uint8_t id = 0)
        : Linear<BGStyle::Color>(owner, name, Component::Type::ProgressBar, id)
    {}
};

/** sta=image: value; bg.bpic; ppic */
template<>
class ProgressBar<BGStyle::Image> : public Drawable {
public:
    enum Tag : uint8_t {
        Val = 192u,
        Ppic,
    };

    void setValue(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"val"}, Tag::Val, v);
    }

    /** mcu: фон полосы — методы в `bg` */
    resources::ProgressBarBackground bg;

    void setPpic(PicId v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"ppic"}, Tag::Ppic, v);
    }

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (bg.onResponse(tag, response))
            return;
        Drawable::onResponse(tag, response);
        (void)tag;
        (void)response;
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        Drawable::onResponse(tag, response);
    }

    ProgressBar(Page& owner, const Literal& name, uint8_t id = 0)
        : Drawable(owner, name, Component::Type::ProgressBar, id)
        , bg{*this}
    {}
};

template<BGStyle S = BGStyle::Color>
class Slider : public Linear<S> {
    static_assert(S != BGStyle::Transparent,
        "Slider: BGStyle::Transparent is not supported");

public:
    /** user: положение ползунка (`Linear::value`) */
    // value — унаследовано от Linear<S>
    /** mcu: бегунок — поля в `cursor` */
    resources::Cursor cursor;
    /** mcu: bco1 — поля в `bg2` */
    resources::Background2 bg2;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (cursor.onResponse(tag, response))
            return;
        if (bg2.onResponse(tag, response))
            return;
        Linear<S>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        Linear<S>::onResponse(tag, response);
    }

    Slider(Page& owner, const Literal& name, uint8_t id = 0)
        : Linear<S>(owner, name, Component::Type::Slider, id)
        , cursor{*this}
        , bg2{*this}
    {}
};

template<BGStyle S = BGStyle::Color>
class Gauge : public Styled<S> {
public:
    enum Tag : uint8_t {
        Val = 192u,
        Format,
        Up,
        Down,
        Left,
        Pco,
        Pco2,
        Hig,
        Vvs0,
        Vvs1,
        Vvs2,
    };

    void setVal(int32_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"val"}, Tag::Val, v);
    }

    /** mcu: формат подписи */
    attr::String<32> format;

    void setPicUp(PicId v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"up"}, Tag::Up, v);
    }

    void setPicDown(PicId v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"down"}, Tag::Down, v);
    }

    void setPicLeft(PicId v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"left"}, Tag::Left, v);
    }

    void setPco(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pco"}, Tag::Pco, v);
    }

    void setPco2(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pco2"}, Tag::Pco2, v);
    }

    void setHig(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"hig"}, Tag::Hig, v);
    }

    /** mcu: подпись vvs0 */
    attr::String<24> vvs0;
    /** mcu: подпись vvs1 */
    attr::String<24> vvs1;
    /** mcu: подпись vvs2 */
    attr::String<24> vvs2;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        Styled<S>::onResponse(tag, response);
        (void)tag;
        (void)response;
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        switch (tag) {
        case Tag::Format:
            format.applyResponse(response);
            return;
        case Tag::Vvs0:
            vvs0.applyResponse(response);
            return;
        case Tag::Vvs1:
            vvs1.applyResponse(response);
            return;
        case Tag::Vvs2:
            vvs2.applyResponse(response);
            return;
        default:
            break;
        }
        Styled<S>::onResponse(tag, response);
    }

    Gauge(Page& owner, const Literal& name, uint8_t id = 0)
        : Styled<S>(owner, name, Component::Type::Gauge, id)
        , format{*this, "format", Tag::Format}
        , vvs0{*this, "vvs0", Tag::Vvs0}
        , vvs1{*this, "vvs1", Tag::Vvs1}
        , vvs2{*this, "vvs2", Tag::Vvs2}
    {}
};

template<BGStyle S = BGStyle::Color, uint16_t TxtMaxL = 256u>
class ComboBox : public ListSelect<S> {
public:
    enum Tag : uint8_t {
        Ycen = 102u,
        Up,
        Pco3,
        Bco1,
        Pco1,
        Dir,
        Qty,
        Vvs0,
        Bco2,
        Pco2,
        Down,
        Mode,
        Wid,
        Vvs1,
        Xcen,
        Txt,
    };

    void enableVerticalCenter() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"ycen"}, Tag::Ycen, true);
    }

    void disableVerticalCenter() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"ycen"}, Tag::Ycen, false);
    }

    void setPicUp(PicId v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"up"}, Tag::Up, v);
    }

    void setPco3(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pco3"}, Tag::Pco3, v);
    }

    void setBco1(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"bco1"}, Tag::Bco1, v);
    }

    void setPco1(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pco1"}, Tag::Pco1, v);
    }

    /** user: направление прокрутки списка */
    attr::Num<uint8_t> list_dir;

    void setQty(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"qty"}, Tag::Qty, v);
    }

    void setVvs0(int16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"vvs0"}, Tag::Vvs0, v);
    }

    void setBco2(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"bco2"}, Tag::Bco2, v);
    }

    void setPco2(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pco2"}, Tag::Pco2, v);
    }

    void setPicDown(PicId v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"down"}, Tag::Down, v);
    }

    void setMode(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"mode"}, Tag::Mode, v);
    }

    void setWid(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"wid"}, Tag::Wid, v);
    }

    void setVvs1(int16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"vvs1"}, Tag::Vvs1, v);
    }

    void enableHorizontalCenter() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"xcen"}, Tag::Xcen, true);
    }

    void disableHorizontalCenter() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"xcen"}, Tag::Xcen, false);
    }

    /** user: текст выбранной строки (отображение) */
    attr::String<TxtMaxL> txt;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (tag == Tag::Dir) {
            list_dir.applyResponse(response);
            return;
        }
        ListSelect<S>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        if (tag == Tag::Txt) {
            txt.applyResponse(response);
            return;
        }
        ListSelect<S>::onResponse(tag, response);
    }

    ComboBox(Page& owner, const Literal& name, uint8_t id = 0)
        : ListSelect<S>(owner, name, Component::Type::ComboBox, id)
        , list_dir{*this, "dir", Tag::Dir}
        , txt{*this, "txt", Tag::Txt}
    {}
};

template<BGStyle S = BGStyle::Color, uint16_t TxtMaxL = 256u>
class Text : public TextComponent<S, TxtMaxL> {
public:
    enum Tag : uint8_t {
        Key = 129u,
        Pw,
    };

    void bindKeyboard() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"key"}, Tag::Key, true);
    }

    void unbindKeyboard() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"key"}, Tag::Key, false);
    }

    void enablePassword() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pw"}, Tag::Pw, true);
    }

    void disablePassword() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pw"}, Tag::Pw, false);
    }

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        TextComponent<S, TxtMaxL>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        TextComponent<S, TxtMaxL>::onResponse(tag, response);
    }

    Text(Page& owner, const Literal& name, uint8_t id = 0)
        : TextComponent<S, TxtMaxL>(owner, name, Component::Type::Text, id)
    {}
};

template<BGStyle S = BGStyle::Color, uint16_t TxtMaxL = 256u>
class ScrollText : public TextComponent<S, TxtMaxL> {
public:
    enum Tag : uint8_t {
        Key = 129u,
        Dir,
        Dis,
        Tim,
        En,
    };

    void bindKeyboard() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"key"}, Tag::Key, true);
    }

    void unbindKeyboard() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"key"}, Tag::Key, false);
    }

    /** user: направление прокрутки текста */
    attr::Num<uint8_t> dir;

    void setDis(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"dis"}, Tag::Dis, v);
    }

    void setTim(uint32_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"tim"}, Tag::Tim, v);
    }

    void enableAutoScroll() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"en"}, Tag::En, true);
    }

    void disableAutoScroll() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"en"}, Tag::En, false);
    }

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (tag == Tag::Dir) {
            dir.applyResponse(response);
            return;
        }
        TextComponent<S, TxtMaxL>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        TextComponent<S, TxtMaxL>::onResponse(tag, response);
    }

    ScrollText(Page& owner, const Literal& name, uint8_t id = 0)
        : TextComponent<S, TxtMaxL>(owner, name, Component::Type::ScrollText, id)
        , dir{*this, "dir", Tag::Dir}
    {}
};

template<BGStyle S = BGStyle::Color, uint16_t TxtMaxL = 256u>
class Button : public ButtonLikeComponent<S, TxtMaxL> {
public:
    Button(Page& owner, const Literal& name, uint8_t id = 0)
        : ButtonLikeComponent<S, TxtMaxL>(owner, name, Component::Type::Button, id) {}
};

template<BGStyle S = BGStyle::Color, uint16_t TxtMaxL = 256u>
class DualStateButton : public ButtonLikeComponent<S, TxtMaxL> {
public:
    enum Tag : uint8_t {
        Val = 148u,
    };

    /** user: состояние кнопки (нажато/отпущено) */
    attr::Num<uint32_t> val;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (tag == Tag::Val) {
            val.applyResponse(response);
            return;
        }
        ButtonLikeComponent<S, TxtMaxL>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        ButtonLikeComponent<S, TxtMaxL>::onResponse(tag, response);
    }

    DualStateButton(Page& owner, const Literal& name, uint8_t id = 0)
        : ButtonLikeComponent<S, TxtMaxL>(owner, name, Component::Type::DualStateButton, id)
        , val{*this, "val", Tag::Val}
    {}
};

template<BGStyle S = BGStyle::Color, BindingKeyboard Keyboard = BindingKeyboard::None>
class Number : public Numeric<S, Keyboard> {
public:
    enum Tag : uint8_t {
        Length = 163u,
    };

    void setLength(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"length"}, Tag::Length, v);
    }

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        Numeric<S, Keyboard>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        Numeric<S, Keyboard>::onResponse(tag, response);
    }

    Number(Page& owner, const Literal& name, uint8_t id = 0)
        : Numeric<S, Keyboard>(owner, name, Component::Type::Number, id)
    {}
};

template<BGStyle S = BGStyle::Color, BindingKeyboard Keyboard = BindingKeyboard::None>
class XFloat : public Numeric<S, Keyboard> {
public:
    /** mcu: vvs0/vvs1 — поля в `point` */
    resources::FloatPoint point;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (point.onResponse(tag, response))
            return;
        Numeric<S, Keyboard>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        Numeric<S, Keyboard>::onResponse(tag, response);
    }

    XFloat(Page& owner, const Literal& name, uint8_t id = 0)
        : Numeric<S, Keyboard>(owner, name, Component::Type::XFloat, id)
        , point{*this}
    {}
};

template<BGStyle S = BGStyle::Color>
class Checkbox : public Selection<S> {
public:
    Checkbox(Page& owner, const Literal& name, uint8_t id = 0)
        : Selection<S>(owner, name, Component::Type::Checkbox, id) {}
};

template<BGStyle S = BGStyle::Color>
class Radio : public Selection<S> {
public:
    Radio(Page& owner, const Literal& name, uint8_t id = 0)
        : Selection<S>(owner, name, Component::Type::Radio, id) {}
};

template<BGStyle S = BGStyle::Color, uint16_t TxtMaxL = 25u>
class ToggleSwitch : public Selection<S> {
public:
    enum Tag : uint8_t {
        Bco2 = 178u,
        Pco2,
        Pco1,
        Font,
        Dis,
        Txt,
    };

    void setBco2(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"bco2"}, Tag::Bco2, v);
    }

    void setPco2(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pco2"}, Tag::Pco2, v);
    }

    void setPco1(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pco1"}, Tag::Pco1, v);
    }

    void setFont(FontId v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"font"}, Tag::Font, v);
    }

    void setDis(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"dis"}, Tag::Dis, v);
    }

    /** mcu: подпись переключателя */
    attr::String<TxtMaxL> txt;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        Selection<S>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        if (tag == Tag::Txt) {
            txt.applyResponse(response);
            return;
        }
        Selection<S>::onResponse(tag, response);
    }

    ToggleSwitch(Page& owner, const Literal& name, uint8_t id = 0)
        : Selection<S>(owner, name, Component::Type::ToggleSwitch, id)
        , txt{*this, "txt", Tag::Txt}
    {}
};

} // namespace comp
} // namespace nex
