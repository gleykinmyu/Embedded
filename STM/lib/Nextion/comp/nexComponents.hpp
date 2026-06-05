#pragma once

#include <cstdint>
#include "../core/nexTypes.hpp"
#include "nexComponentBase.hpp"
#include "nexCompImpl.hpp"
#include "resources/cursor.hpp"
#include "resources/comboBox.hpp"
#include "resources/gauge.hpp"
#include "resources/progressBar.hpp"
#include "resources/waveform.hpp"

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

    void setPeriod(uint32_t v) noexcept
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

    Timer(Page& owner, const Literal& name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Timer, id)
    {}
};

/** NIS `va` при sta=Number: `val`. Тот же `type`=52, что у StringVariable. */
class NumericVar : public Component {
public:
    enum Tag : uint8_t {
        Val = 192u,
    };

    /** user: значение глобальной числовой переменной */
    attr::Num<int32_t> val;

    using Component::onResponse;
    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (tag == Tag::Val) {
            val.applyResponse(response);
            return;
        }
        Component::onResponse(tag, response);
    }

    NumericVar(Page& owner, const Literal& name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Variable, id)
        , val{*this, "val", Tag::Val}
    {}
};

/** NIS `va` при sta=String: `txt`. Тот же `type`=52, что у NumericVariable. */
template<uint16_t TxtMaxL = 16u>
class StringVar : public Component {
public:
    enum Tag : uint8_t {
        Txt = 192u,
    };

    /** user: значение глобальной строковой переменной */
    attr::String<TxtMaxL> txt;

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        if (tag == Tag::Txt) {
            txt.applyResponse(response);
            return;
        }
        Component::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        Component::onResponse(tag, response);
    }

    StringVar(Page& owner, const Literal& name, uint8_t id = 0)
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

    void setPenColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pco"}, Tag::Pco, v);
    }

    //TODO проверить реальное назначение атрибута dis
    void setDataSpacing(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"dis"}, Tag::Dis, v);
    }

    void setText(const char* text) noexcept
    {
        attr_detail::assignText(*this, Literal{"txt"}, Tag::Txt, text);
    }

    QRCode(Page& owner, const Literal& name, uint8_t id = 0)
        : Styled<BGStyle::Color>(owner, name, Component::Type::QRCode, id)
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

    CropPicture(Page& owner, const Literal& name, uint8_t id = 0)
        : Styled<BGStyle::CropImage>(owner, name, Component::Type::CropPicture, id)
    {}
};

template<BGStyle S = BGStyle::Color, uint8_t ChannelCount = 1u>
class Waveform : public Drawable {
public:
    static constexpr BGStyle kStyle = S;

    enum Tag : uint8_t {
        Dis = 196u,
    };

    /** mcu: фон (`bco`/`pic`/`picc`, `gdc`/`gdw`/`gdh` по `S`) — поля внутри `wfBackground` */
    resources::WfBackground<S> bg;

    /** mcu: каналы (`pco0`…`pco3`, `add`, `addt`); `ChannelCount` задаётся шаблоном (1…4). */
    resources::WaveformChannels<ChannelCount> ch;

    static constexpr uint16_t kDataScaleMin = 10u;
    static constexpr uint16_t kDataScaleMax = 1000u;

    void setDataScale(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"dis"}, Tag::Dis,
            attr_detail::clamp(v, kDataScaleMin, kDataScaleMax));
    }

    Waveform(Page& owner, const Literal& name, uint8_t id = 0)
        : Drawable(owner, name, Component::Type::Waveform, id)
        , bg{*this}
        , ch{*this}
    {}
};

template<BGStyle S = BGStyle::Color>
class ProgressBar : public Drawable {
    static_assert(S == BGStyle::Color || S == BGStyle::Image,
        "ProgressBar: only BGStyle::Color and BGStyle::Image are supported");

public:
    static constexpr BGStyle kStyle = S;

    enum Tag : uint8_t {
        Val = 192u,
    };

    /** mcu: уровень заливки */
    attr::Num<uint8_t> value;

    /** mcu: фон (`bco` / `bpic`) — поля внутри `bg` */
    resources::PbBackground<S> bg;

    /** mcu: заливка (`pco`/`dis` / `ppic`) — поля внутри `bar` */
    resources::PbBar<S> bar;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (tag == Tag::Val) {
            value.applyResponse(response);
            return;
        }
        TouchArea::onResponse(tag, response);
    }

    ProgressBar(Page& owner, const Literal& name, uint8_t id = 0)
        : Drawable(owner, name, Component::Type::ProgressBar, id)
        , value{*this, "val", Tag::Val}
        , bg{*this}
        , bar{*this}
    {}
};

template<BGStyle S = BGStyle::Color>
class Slider : public Styled<S> {
    static_assert(S != BGStyle::Transparent,
        "Slider: BGStyle::Transparent is not supported");

public:
    enum Tag : uint8_t {
        Val = 192u,
    };

    /** user: положение ползунка */
    attr::Num<uint8_t> value;

    /** mcu: бегунок — поля в `cursor` */
    resources::Cursor cursor;
    /** mcu: bco1 — поля в `bg2` */
    resources::Background<S, 1u> bg2;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (tag == Tag::Val) {
            value.applyResponse(response);
            return;
        }
        TouchArea::onResponse(tag, response);
    }

    Slider(Page& owner, const Literal& name, uint8_t id = 0)
        : Styled<S>(owner, name, Component::Type::Slider, id)
        , value{*this, "val", Tag::Val}
        , cursor{*this}
        , bg2{*this}
    {}
};

template<BGStyle S = BGStyle::Color>
class Gauge : public Styled<S> {
public:
    enum Tag : uint8_t {
        Val = 192u,
    };

    static constexpr uint16_t kAngleMin = 0;
    static constexpr uint16_t kAngleMax = 360;

    /** mcu: центр шкалы — поля внутри `center` */
    resources::GaugeCenter center;

    /** mcu: стрелка — поля внутри `pointer` */
    resources::GaugePointer pointer;

    void setAngle(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"val"}, Tag::Val,
            attr_detail::clamp(v, kAngleMin, kAngleMax));
    }

    Gauge(Page& owner, const Literal& name, uint8_t id = 0)
        : Styled<S>(owner, name, Component::Type::Gauge, id)
        , center{*this}
        , pointer{*this}
    {}
};

template<BGStyle S = BGStyle::Color>
class ComboBox : public ListSelect<S> {
public:
    enum Tag : uint8_t {
        IsOpened = 102u,
        Ycen,
        Xcen,
    };

    void setVAlign(VAlign v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"ycen"}, Tag::Ycen, v);
    }

    void setHAlign(HAlign v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"xcen"}, Tag::Xcen, v);
    }

    /** mcu: рамка — поля внутри `border` */
    resources::ComboBorder border;

    /** mcu: стрелка — поля внутри `arrow` */
    resources::ComboArrow arrow;

    /** mcu: ячейки списка — поля внутри `cells` */
    resources::ComboCells cells;

    /** user: список раскрыт (NIS `down`) */
    attr::Num<bool> isOpened;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (tag == Tag::IsOpened) {
            isOpened.applyResponse(response);
            return;
        }
        ListSelect<S>::onResponse(tag, response);
    }

    ComboBox(Page& owner, const Literal& name, uint8_t id = 0)
        : ListSelect<S>(owner, name, Component::Type::ComboBox, id)
        , border{*this}
        , arrow{*this}
        , cells{*this}
        , isOpened{*this, "down", Tag::IsOpened}
    {}
};

template<BGStyle S = BGStyle::Color>
class TextSelect : public ListSelect<S> {
public:
    enum Tag : uint8_t {
        Pco2 = 104u,
        Pco1,
        Dis,
        Txt,
    };

    void setSelColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pco2"}, Tag::Pco2, v);
    }

    void setLineColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pco1"}, Tag::Pco1, v);
    }

    void setSelectionLine(bool enabled) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"dis"}, Tag::Dis, enabled);
    }

    /** user: текст выбранной строки (RO) */
    //attr::StringRO<256> txt;

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        if (tag == Tag::Txt) {
            //txt.applyResponse(response);
            return;
        }
        ListSelect<S>::onResponse(tag, response);
    }

    TextSelect(Page& owner, const Literal& name, uint8_t id = 0)
        : ListSelect<S>(owner, name, Component::Type::TextSelect, id)
        //, txt{*this, "txt", Tag::Txt}
    {}
};

template<BGStyle S = BGStyle::Color, uint16_t TxtMaxL = 16u>
class Text : public TextComponent<S, TxtMaxL> {
public:
    enum Tag : uint8_t {
        Pw = 129u,
    };

    void enablePassword() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pw"}, Tag::Pw, true);
    }

    void disablePassword() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pw"}, Tag::Pw, false);
    }

    Text(Page& owner, const Literal& name, uint8_t id = 0)
        : TextComponent<S, TxtMaxL>(owner, name, Component::Type::Text, id)
    {}
};

template<BGStyle S = BGStyle::Color, uint16_t TxtMaxL = 256u>
class SLText : public TextComponent<S, TxtMaxL> {
public:
    enum Tag : uint8_t {
        Left = 129u,
        Ch,
        ValY,
        MaxvalY,
    };

    void setVAlign(VAlign) = delete;

    /** user: смещение текста по горизонтали */
    attr::Num<uint16_t> left;
    /** user: канал / индекс строки */
    attr::Num<uint8_t> ch;
    /** user: позиция прокрутки по Y */
    attr::Num<uint16_t> val_y;
    /** user: максимум прокрутки по Y (RO) */
    attr::NumRO<uint16_t> maxval_y;

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case Tag::Left:
            left.applyResponse(response);
            return;
        case Tag::Ch:
            ch.applyResponse(response);
            return;
        case Tag::ValY:
            val_y.applyResponse(response);
            return;
        case Tag::MaxvalY:
            maxval_y.applyResponse(response);
            return;
        default:
            break;
        }
        TextComponent<S, TxtMaxL>::onResponse(tag, response);
    }

    SLText(Page& owner, const Literal& name, uint8_t id = 0)
        : TextComponent<S, TxtMaxL>(owner, name, Component::Type::SLText, id)
        , left{*this, "left", Tag::Left}
        , ch{*this, "ch", Tag::Ch}
        , val_y{*this, "val_y", Tag::ValY}
        , maxval_y{*this, "maxval_y", Tag::MaxvalY}
    {}
};

template<BGStyle S = BGStyle::Color, uint16_t TxtMaxL = 16u>
class ScrollText : public TextComponent<S, TxtMaxL> {
public:
    enum Tag : uint8_t {
        Dir = 129u,
        Dis,
        Tim,
        En,
    };

    static constexpr uint16_t kPeriodMin = 80u;
    static constexpr uint16_t kPeriodMax = 65535u;
    static constexpr uint8_t kScrollStepMin = 2u;
    static constexpr uint8_t kScrollStepMax = 50u;

    void setScrollDirection(ScrollDirection v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"dir"}, Tag::Dir, static_cast<uint8_t>(v));
    }

    void setScrollStep(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"dis"}, Tag::Dis,
            attr_detail::clamp(v, kScrollStepMin, kScrollStepMax));
    }

    void setPeriod(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"tim"}, Tag::Tim,
            attr_detail::clamp(v, kPeriodMin, kPeriodMax));
    }

    void enable() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"en"}, Tag::En, true);
    }

    void disable() noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"en"}, Tag::En, false);
    }

    ScrollText(Page& owner, const Literal& name, uint8_t id = 0)
        : TextComponent<S, TxtMaxL>(owner, name, Component::Type::ScrollText, id)
    {}
};

template<BGStyle S = BGStyle::Color, uint16_t TxtMaxL = 16u>
class Button : public ButtonLikeComponent<S, TxtMaxL> {
public:
    Button(Page& owner, const Literal& name, uint8_t id = 0)
        : ButtonLikeComponent<S, TxtMaxL>(owner, name, Component::Type::Button, id) {}
};

template<BGStyle S = BGStyle::Color, uint16_t TxtMaxL = 16u>
class DualStateButton : public ButtonLikeComponent<S, TxtMaxL> {
public:
    enum Tag : uint8_t {
        Val = 148u,
    };

    /** user: состояние кнопки (нажато/отпущено) */
    attr::Num<uint32_t> val;

    using ButtonLikeComponent<S, TxtMaxL>::onResponse;
    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        if (tag == Tag::Val) {
            val.applyResponse(response);
            return;
        }
        ButtonLikeComponent<S, TxtMaxL>::onResponse(tag, response);
    }

    DualStateButton(Page& owner, const Literal& name, uint8_t id = 0)
        : ButtonLikeComponent<S, TxtMaxL>(owner, name, Component::Type::DualStateButton, id)
        , val{*this, "val", Tag::Val}
    {}
};

template<BGStyle S = BGStyle::Color>
class Number : public Numeric<S> {
public:
    enum Tag : uint8_t {
        Length = 163u,
        Format,
    };

    void setDigitCount(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"length"}, Tag::Length, v);
    }

    void setFormat(NumFormat v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"format"}, Tag::Format, v);
    }

    Number(Page& owner, const Literal& name, uint8_t id = 0)
        : Numeric<S>(owner, name, Component::Type::Number, id)
    {}
};

template<BGStyle S = BGStyle::Color>
class XFloat : public Numeric<S> {
public:
    enum Tag : uint8_t {
        FormatBefore = 163u,
        FormatAfter,
    };

    /** mcu: формат числа — знаки до и после точки (NIS `vvs0`, `vvs1`) */
    void setFormat(uint8_t digitsBeforePoint, uint8_t digitsAfterPoint) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"vvs0"}, Tag::FormatBefore, digitsBeforePoint);
        attr_detail::assignNumeric(*this, Literal{"vvs1"}, Tag::FormatAfter, digitsAfterPoint);
    }

    XFloat(Page& owner, const Literal& name, uint8_t id = 0)
        : Numeric<S>(owner, name, Component::Type::XFloat, id)
    {}
};

template<BGStyle S = BGStyle::Color>
class DataRecord : public DataFile<S> {
public:
    enum Tag : uint8_t {
        Path = 80u,
        Lenth,
        Maxval,
        Length,
        Format,
        Mode,
        Order,
        Hig,
        Gdc,
        Gdw,
        Gdh,
        Bco1,
        Pco1,
        Xcen,
    };

    /** mcu: путь к файлу данных */
    attr::String<512> path;
    /** user: длина файла (RO) */
    attr::NumRO<uint32_t> lenth;
    /** user: максимальное значение (RO) */
    attr::NumRO<int32_t> maxval;

    void setRecordLength(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"length"}, Tag::Length, v);
    }

    void setFormat(NumFormat v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"format"}, Tag::Format, v);
    }

    void setMode(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"mode"}, Tag::Mode, v);
    }

    void setOrder(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"order"}, Tag::Order, v);
    }

    void setCellSize(uint8_t v) noexcept
    {
        static constexpr uint8_t kMin = 16u;
        static constexpr uint8_t kMax = 255u;
        attr_detail::assignNumeric(*this, Literal{"hig"}, Tag::Hig,
            attr_detail::clamp(v, kMin, kMax));
    }

    void setGridColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"gdc"}, Tag::Gdc, v);
    }

    void setGridWidth(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"gdw"}, Tag::Gdw, v);
    }

    void setGridHeight(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"gdh"}, Tag::Gdh, v);
    }

    void setCellBgColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"bco1"}, Tag::Bco1, v);
    }

    void setCellColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"pco1"}, Tag::Pco1, v);
    }

    void setHAlign(HAlign v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"xcen"}, Tag::Xcen, v);
    }

    void onResponse(uint8_t tag, const msg::getNumeric& response) override
    {
        switch (tag) {
        case Tag::Lenth:
            lenth.applyResponse(response);
            return;
        case Tag::Maxval:
            maxval.applyResponse(response);
            return;
        default:
            break;
        }
        DataFile<S>::onResponse(tag, response);
    }

    void onResponse(uint8_t tag, const msg::getString& response) override
    {
        if (tag == Tag::Path) {
            path.applyResponse(response);
            return;
        }
        DataFile<S>::onResponse(tag, response);
    }

    DataRecord(Page& owner, const Literal& name, uint8_t id = 0)
        : DataFile<S>(owner, name, Component::Type::DataRecord, id)
        , path{*this, "path", Tag::Path}
        , lenth{*this, "lenth", Tag::Lenth}
        , maxval{*this, "maxval", Tag::Maxval}
    {}
};

class Checkbox : public Selection {
public:
    Checkbox(Page& owner, const Literal& name, uint8_t id = 0)
        : Selection(owner, name, Component::Type::Checkbox, id) {}
};

class Radio : public Selection {
public:
    Radio(Page& owner, const Literal& name, uint8_t id = 0)
        : Selection(owner, name, Component::Type::Radio, id) {}
};

class ToggleSwitch : public Selection {
public:
    enum Tag : uint8_t {
        Dis = 180u,
        Txt,
    };

    /** mcu: состояние «вкл» — `pressed.bg` → `bco2`, `pressed.setMarkerColor` → `pco2` */
    resources::PressedMarker pressed;

    /** mcu: шрифт подписи (`font`, `pco1`) — поля внутри `font` */
    resources::FontId<1u> font;

    void setLabelGap(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, Literal{"dis"}, Tag::Dis, v);
    }

    /** mcu: подпись переключателя */
    //todo: сделать setter с двумя const char* которые склеиваются в одну строку через "/" - неясно? нужен временный буффер???
    //возможно использовать txt как private поле
    attr::String<24> txt;

    ToggleSwitch(Page& owner, const Literal& name, uint8_t id = 0)
        : Selection(owner, name, Component::Type::ToggleSwitch, id)
        , pressed{*this}
        , font{*this}
        , txt{*this, "txt", Tag::Txt}
    {}
};


} // namespace comp
} // namespace nex
