#pragma once

#include <cstdint>
#include "../core/nexTypes.hpp"
#include "nexComponentBase.hpp"
#include "nexCompImpl.hpp"

namespace nex {
// --- Прямые наследники Component (без x,y,w,h на странице) --------------------

/** tim, en */
class Timer : public Component {
public:
    enum Tag : uint8_t {
        Tim = 192u,
        En,
    };

    attr::Num<uint32_t> tim;
    attr::Num<bool> en;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case Tag::Tim:
            tim.applyResponse(response);
            return;
        case Tag::En:
            en.applyResponse(response);
            return;
        default:
            break;
        }
        Component::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        Component::onResponse(tag, response);
    }

    Timer(Page& owner, const Literal& name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Timer, id)
        , tim{*this, "tim", Tag::Tim}
        , en{*this, "en", Tag::En}
    {}
};

/** NIS `va` при sta=Number: `val`. Тот же `type`=52, что у StringVariable. */
class NumericVariable : public Component {
public:
    enum Tag : uint8_t {
        Val = 192u,
    };

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

class Hotspot : public GeometryComponent {
public:
    Hotspot(Page& owner, const Literal& name, uint8_t id = 0)
        : GeometryComponent(owner, name, Component::Type::Hotspot, id) {}
};

/** pco; dis; txt (данные QR) */
class QRCode : public BGComponent<BGStyle::Color> {
public:
    enum Tag : uint8_t {
        Pco = 192u,
        Dis,
        Txt,
    };

    attr::Num<nex::Color> pco;
    attr::Num<uint16_t> dis;
    attr::String<256> txt;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case Tag::Pco:
            pco.applyResponse(response);
            return;
        case Tag::Dis:
            dis.applyResponse(response);
            return;
        default:
            break;
        }
        BGComponent<BGStyle::Color>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        if (tag == Tag::Txt) {
            txt.applyResponse(response);
            return;
        }
        BGComponent<BGStyle::Color>::onResponse(tag, response);
    }

    QRCode(Page& owner, const Literal& name, uint8_t id = 0)
        : BGComponent<BGStyle::Color>(owner, name, Component::Type::QRCode, id)
        , pco{*this, "pco", Tag::Pco}
        , dis{*this, "dis", Tag::Dis}
        , txt{*this, "txt", Tag::Txt}
    {}
};

class Picture : public BGComponent<BGStyle::Image> {
public:
    Picture(Page& owner, const Literal& name, uint8_t id = 0)
        : BGComponent<BGStyle::Image>(owner, name, Component::Type::Picture, id) {}
};

class CropPicture : public BGComponent<BGStyle::CropImage> {
public:
    enum Tag : uint8_t {
        Cpic = 192u,
    };

    attr::Num<PicId> crop;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (tag == Tag::Cpic) {
            crop.applyResponse(response);
            return;
        }
        BGComponent<BGStyle::CropImage>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        BGComponent<BGStyle::CropImage>::onResponse(tag, response);
    }

    CropPicture(Page& owner, const Literal& name, uint8_t id = 0)
        : BGComponent<BGStyle::CropImage>(owner, name, Component::Type::CropPicture, id)
        , crop{*this, "cpic", Tag::Cpic}
    {}
};

template<BGStyle S = BGStyle::Color>
class Waveform : public DrawableColoredComponent<S> {
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

    attr::Num<uint8_t> ch;
    attr::Num<nex::Color> pco0;
    attr::Num<nex::Color> pco1;
    attr::Num<nex::Color> pco2;
    attr::Num<nex::Color> pco3;
    attr::Num<uint16_t> gdc;
    attr::Num<uint16_t> gdw;
    attr::Num<uint16_t> gdh;
    attr::Num<uint8_t> dis;
    attr::Num<uint16_t> wid;
    attr::Num<uint16_t> hig;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case Tag::Ch:
            ch.applyResponse(response);
            return;
        case Tag::Pco0:
            pco0.applyResponse(response);
            return;
        case Tag::Pco1:
            pco1.applyResponse(response);
            return;
        case Tag::Pco2:
            pco2.applyResponse(response);
            return;
        case Tag::Pco3:
            pco3.applyResponse(response);
            return;
        case Tag::Gdc:
            gdc.applyResponse(response);
            return;
        case Tag::Gdw:
            gdw.applyResponse(response);
            return;
        case Tag::Gdh:
            gdh.applyResponse(response);
            return;
        case Tag::Dis:
            dis.applyResponse(response);
            return;
        case Tag::Wid:
            wid.applyResponse(response);
            return;
        case Tag::Hig:
            hig.applyResponse(response);
            return;
        default:
            break;
        }
        DrawableColoredComponent<S>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        DrawableColoredComponent<S>::onResponse(tag, response);
    }

    Waveform(Page& owner, const Literal& name, uint8_t id = 0)
        : DrawableColoredComponent<S>(owner, name, Component::Type::Waveform, id)
        , ch{*this, "ch", Tag::Ch}
        , pco0{*this, "pco0", Tag::Pco0}
        , pco1{*this, "pco1", Tag::Pco1}
        , pco2{*this, "pco2", Tag::Pco2}
        , pco3{*this, "pco3", Tag::Pco3}
        , gdc{*this, "gdc", Tag::Gdc}
        , gdw{*this, "gdw", Tag::Gdw}
        , gdh{*this, "gdh", Tag::Gdh}
        , dis{*this, "dis", Tag::Dis}
        , wid{*this, "wid", Tag::Wid}
        , hig{*this, "hig", Tag::Hig}
    {}
};

template<BGStyle S = BGStyle::Color>
class ProgressBar : public DrawableColoredComponent<S> {
public:
    enum Tag : uint8_t {
        Val = 192u,
        Dis,
        Pco,
        Bpic,
        Ppic,
    };

    attr::Num<uint32_t> val;
    attr::Num<uint16_t> dis;
    attr::Num<nex::Color> pco;
    attr::Num<PicId> bpic;
    attr::Num<PicId> ppic;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case Tag::Val:
            val.applyResponse(response);
            return;
        case Tag::Dis:
            dis.applyResponse(response);
            return;
        case Tag::Pco:
            pco.applyResponse(response);
            return;
        case Tag::Bpic:
            bpic.applyResponse(response);
            return;
        case Tag::Ppic:
            ppic.applyResponse(response);
            return;
        default:
            break;
        }
        DrawableColoredComponent<S>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        DrawableColoredComponent<S>::onResponse(tag, response);
    }

    ProgressBar(Page& owner, const Literal& name, uint8_t id = 0)
        : DrawableColoredComponent<S>(owner, name, Component::Type::ProgressBar, id)
        , val{*this, "val", Tag::Val}
        , dis{*this, "dis", Tag::Dis}
        , pco{*this, "pco", Tag::Pco}
        , bpic{*this, "bpic", Tag::Bpic}
        , ppic{*this, "ppic", Tag::Ppic}
    {}
};

template<BGStyle S = BGStyle::Color>
class Slider : public DrawableColoredComponent<S> {
public:
    enum Tag : uint8_t {
        Wid = 192u,
        Hig,
        Pic1,
        Picc1,
        Pco,
        Val,
        Maxval,
        Minval,
        Ch,
    };

    attr::Num<uint16_t> wid;
    attr::Num<uint16_t> hig;
    attr::Num<PicId> pic1;
    attr::Num<PicId> picc1;
    attr::Num<nex::Color> pco;
    attr::Num<uint32_t> val;
    attr::Num<uint16_t> maxval;
    attr::Num<uint16_t> minval;
    attr::Num<uint8_t> ch;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case Tag::Wid:
            wid.applyResponse(response);
            return;
        case Tag::Hig:
            hig.applyResponse(response);
            return;
        case Tag::Pic1:
            pic1.applyResponse(response);
            return;
        case Tag::Picc1:
            picc1.applyResponse(response);
            return;
        case Tag::Pco:
            pco.applyResponse(response);
            return;
        case Tag::Val:
            val.applyResponse(response);
            return;
        case Tag::Maxval:
            maxval.applyResponse(response);
            return;
        case Tag::Minval:
            minval.applyResponse(response);
            return;
        case Tag::Ch:
            ch.applyResponse(response);
            return;
        default:
            break;
        }
        DrawableColoredComponent<S>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        DrawableColoredComponent<S>::onResponse(tag, response);
    }

    Slider(Page& owner, const Literal& name, uint8_t id = 0)
        : DrawableColoredComponent<S>(owner, name, Component::Type::Slider, id)
        , wid{*this, "wid", Tag::Wid}
        , hig{*this, "hig", Tag::Hig}
        , pic1{*this, "pic1", Tag::Pic1}
        , picc1{*this, "picc1", Tag::Picc1}
        , pco{*this, "pco", Tag::Pco}
        , val{*this, "val", Tag::Val}
        , maxval{*this, "maxval", static_cast<uint16_t>(65535), Tag::Maxval}
        , minval{*this, "minval", Tag::Minval}
        , ch{*this, "ch", Tag::Ch}
    {}
};

template<BGStyle S = BGStyle::Color>
class Gauge : public DrawableColoredComponent<S> {
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

    attr::Num<int32_t> val;
    attr::String<32> format;
    attr::Num<PicId> pic_up;
    attr::Num<PicId> pic_down;
    attr::Num<PicId> pic_left;
    attr::Num<nex::Color> pco;
    attr::Num<nex::Color> pco2;
    attr::Num<uint16_t> hig;
    attr::String<24> vvs0;
    attr::String<24> vvs1;
    attr::String<24> vvs2;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case Tag::Val:
            val.applyResponse(response);
            return;
        case Tag::Up:
            pic_up.applyResponse(response);
            return;
        case Tag::Down:
            pic_down.applyResponse(response);
            return;
        case Tag::Left:
            pic_left.applyResponse(response);
            return;
        case Tag::Pco:
            pco.applyResponse(response);
            return;
        case Tag::Pco2:
            pco2.applyResponse(response);
            return;
        case Tag::Hig:
            hig.applyResponse(response);
            return;
        default:
            break;
        }
        DrawableColoredComponent<S>::onResponse(tag, response);
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
        DrawableColoredComponent<S>::onResponse(tag, response);
    }

    Gauge(Page& owner, const Literal& name, uint8_t id = 0)
        : DrawableColoredComponent<S>(owner, name, Component::Type::Gauge, id)
        , val{*this, "val", Tag::Val}
        , format{*this, "format", Tag::Format}
        , pic_up{*this, "up", Tag::Up}
        , pic_down{*this, "down", Tag::Down}
        , pic_left{*this, "left", Tag::Left}
        , pco{*this, "pco", Tag::Pco}
        , pco2{*this, "pco2", Tag::Pco2}
        , hig{*this, "hig", Tag::Hig}
        , vvs0{*this, "vvs0", Tag::Vvs0}
        , vvs1{*this, "vvs1", Tag::Vvs1}
        , vvs2{*this, "vvs2", Tag::Vvs2}
    {}
};

template<BGStyle S = BGStyle::Color, uint16_t TxtMaxL = 256u>
class ComboBox : public ListSelectTextComponent<S> {
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

    attr::Num<bool> ycen;
    attr::Num<PicId> pic_up;
    attr::Num<nex::Color> pco3;
    attr::Num<nex::Color> bco1;
    attr::Num<nex::Color> pco1;
    attr::Num<uint8_t> list_dir;
    attr::Num<uint16_t> qty;
    attr::Num<int16_t> vvs0;
    attr::Num<nex::Color> bco2;
    attr::Num<nex::Color> pco2;
    attr::Num<PicId> pic_down;
    attr::Num<uint8_t> mode;
    attr::Num<uint16_t> wid;
    attr::Num<int16_t> vvs1;
    attr::Num<bool> xcen;
    attr::String<TxtMaxL> txt;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case Tag::Ycen:
            ycen.applyResponse(response);
            return;
        case Tag::Up:
            pic_up.applyResponse(response);
            return;
        case Tag::Pco3:
            pco3.applyResponse(response);
            return;
        case Tag::Bco1:
            bco1.applyResponse(response);
            return;
        case Tag::Pco1:
            pco1.applyResponse(response);
            return;
        case Tag::Dir:
            list_dir.applyResponse(response);
            return;
        case Tag::Qty:
            qty.applyResponse(response);
            return;
        case Tag::Vvs0:
            vvs0.applyResponse(response);
            return;
        case Tag::Bco2:
            bco2.applyResponse(response);
            return;
        case Tag::Pco2:
            pco2.applyResponse(response);
            return;
        case Tag::Down:
            pic_down.applyResponse(response);
            return;
        case Tag::Mode:
            mode.applyResponse(response);
            return;
        case Tag::Wid:
            wid.applyResponse(response);
            return;
        case Tag::Vvs1:
            vvs1.applyResponse(response);
            return;
        case Tag::Xcen:
            xcen.applyResponse(response);
            return;
        default:
            break;
        }
        ListSelectTextComponent<S>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        if (tag == Tag::Txt) {
            txt.applyResponse(response);
            return;
        }
        ListSelectTextComponent<S>::onResponse(tag, response);
    }

    ComboBox(Page& owner, const Literal& name, uint8_t id = 0)
        : ListSelectTextComponent<S>(owner, name, Component::Type::ComboBox, id)
        , ycen{*this, "ycen", Tag::Ycen}
        , pic_up{*this, "up", Tag::Up}
        , pco3{*this, "pco3", Tag::Pco3}
        , bco1{*this, "bco1", Tag::Bco1}
        , pco1{*this, "pco1", Tag::Pco1}
        , list_dir{*this, "dir", Tag::Dir}
        , qty{*this, "qty", Tag::Qty}
        , vvs0{*this, "vvs0", Tag::Vvs0}
        , bco2{*this, "bco2", Tag::Bco2}
        , pco2{*this, "pco2", Tag::Pco2}
        , pic_down{*this, "down", Tag::Down}
        , mode{*this, "mode", Tag::Mode}
        , wid{*this, "wid", Tag::Wid}
        , vvs1{*this, "vvs1", Tag::Vvs1}
        , xcen{*this, "xcen", Tag::Xcen}
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

    attr::Num<bool> key;
    attr::Num<bool> pw;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case Tag::Key:
            key.applyResponse(response);
            return;
        case Tag::Pw:
            pw.applyResponse(response);
            return;
        default:
            break;
        }
        TextComponent<S, TxtMaxL>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        TextComponent<S, TxtMaxL>::onResponse(tag, response);
    }

    Text(Page& owner, const Literal& name, uint8_t id = 0)
        : TextComponent<S, TxtMaxL>(owner, name, Component::Type::Text, id)
        , key{*this, "key", Tag::Key}
        , pw{*this, "pw", Tag::Pw}
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

    attr::Num<bool> key;
    attr::Num<uint8_t> dir;
    attr::Num<uint16_t> dis;
    attr::Num<uint32_t> tim;
    attr::Num<bool> en;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case Tag::Key:
            key.applyResponse(response);
            return;
        case Tag::Dir:
            dir.applyResponse(response);
            return;
        case Tag::Dis:
            dis.applyResponse(response);
            return;
        case Tag::Tim:
            tim.applyResponse(response);
            return;
        case Tag::En:
            en.applyResponse(response);
            return;
        default:
            break;
        }
        TextComponent<S, TxtMaxL>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        TextComponent<S, TxtMaxL>::onResponse(tag, response);
    }

    ScrollText(Page& owner, const Literal& name, uint8_t id = 0)
        : TextComponent<S, TxtMaxL>(owner, name, Component::Type::ScrollText, id)
        , key{*this, "key", Tag::Key}
        , dir{*this, "dir", Tag::Dir}
        , dis{*this, "dis", Tag::Dis}
        , tim{*this, "tim", Tag::Tim}
        , en{*this, "en", Tag::En}
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
class Number : public NumericComponent<S, Keyboard> {
public:
    enum Tag : uint8_t {
        Length = 163u,
    };

    attr::Num<uint16_t> length;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (tag == Tag::Length) {
            length.applyResponse(response);
            return;
        }
        NumericComponent<S, Keyboard>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        NumericComponent<S, Keyboard>::onResponse(tag, response);
    }

    Number(Page& owner, const Literal& name, uint8_t id = 0)
        : NumericComponent<S, Keyboard>(owner, name, Component::Type::Number, id)
        , length{*this, "length", Tag::Length}
    {}
};

template<BGStyle S = BGStyle::Color, BindingKeyboard Keyboard = BindingKeyboard::None>
class XFloat : public NumericComponent<S, Keyboard> {
public:
    enum Tag : uint8_t {
        Vvs0 = 163u,
        Vvs1,
    };

    attr::String<24> vvs0;
    attr::String<24> vvs1;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        NumericComponent<S, Keyboard>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        switch (tag) {
        case Tag::Vvs0:
            vvs0.applyResponse(response);
            return;
        case Tag::Vvs1:
            vvs1.applyResponse(response);
            return;
        default:
            break;
        }
        NumericComponent<S, Keyboard>::onResponse(tag, response);
    }

    XFloat(Page& owner, const Literal& name, uint8_t id = 0)
        : NumericComponent<S, Keyboard>(owner, name, Component::Type::XFloat, id)
        , vvs0{*this, "vvs0", Tag::Vvs0}
        , vvs1{*this, "vvs1", Tag::Vvs1}
    {}
};

template<BGStyle S = BGStyle::Color>
class Checkbox : public SelectionComponent<S> {
public:
    Checkbox(Page& owner, const Literal& name, uint8_t id = 0)
        : SelectionComponent<S>(owner, name, Component::Type::Checkbox, id) {}
};

template<BGStyle S = BGStyle::Color>
class Radio : public SelectionComponent<S> {
public:
    Radio(Page& owner, const Literal& name, uint8_t id = 0)
        : SelectionComponent<S>(owner, name, Component::Type::Radio, id) {}
};

template<BGStyle S = BGStyle::Color, uint16_t TxtMaxL = 25u>
class ToggleSwitch : public SelectionComponent<S> {
public:
    enum Tag : uint8_t {
        Bco2 = 178u,
        Pco2,
        Pco1,
        Font,
        Dis,
        Txt,
    };

    attr::Num<nex::Color> bco2;
    attr::Num<nex::Color> pco2;
    attr::Num<nex::Color> pco1;
    attr::Num<FontId> font;
    attr::Num<uint16_t> dis;
    attr::String<TxtMaxL> txt;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case Tag::Bco2:
            bco2.applyResponse(response);
            return;
        case Tag::Pco2:
            pco2.applyResponse(response);
            return;
        case Tag::Pco1:
            pco1.applyResponse(response);
            return;
        case Tag::Font:
            font.applyResponse(response);
            return;
        case Tag::Dis:
            dis.applyResponse(response);
            return;
        default:
            break;
        }
        SelectionComponent<S>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        if (tag == Tag::Txt) {
            txt.applyResponse(response);
            return;
        }
        SelectionComponent<S>::onResponse(tag, response);
    }

    ToggleSwitch(Page& owner, const Literal& name, uint8_t id = 0)
        : SelectionComponent<S>(owner, name, Component::Type::ToggleSwitch, id)
        , bco2{*this, "bco2", Tag::Bco2}
        , pco2{*this, "pco2", Tag::Pco2}
        , pco1{*this, "pco1", Tag::Pco1}
        , font{*this, "font", Tag::Font}
        , dis{*this, "dis", Tag::Dis}
        , txt{*this, "txt", Tag::Txt}
    {}
};

} // namespace nex
