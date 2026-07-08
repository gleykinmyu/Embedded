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

/**
 * Публичные классы виджетов (`nex::comp::*`): листья иерархии из `nexCompImpl.hpp`.
 * Атрибуты `user` / `mcu` — см. `nexAttributes.hpp`; коды типов — `Component::Type`.
 */

namespace nex {
namespace comp {
// --- Прямые наследники Component (без x,y,w,h на странице) --------------------

/** tim, en */
class Timer : public Component {
public:
    void setPeriod(uint32_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Tim, v);
    }

    void enable() noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::En, true);
    }

    void disable() noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::En, false);
    }

    Timer(IPage& owner, const Literal& name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Timer, id)
    {}
};

/** NIS `va` при sta=Number: `val`. Тот же `type`=52, что у StringVariable. */
class NumericVar : public Component {
public:
    /** user: значение глобальной числовой переменной */
    attr::Num<int32_t> val;

    using Component::onResponse;
    void onResponse(const msg::getNumeric& response, uint8_t tag) override
    {
        if (tag == static_cast<uint8_t>(attr::Id::Val)) {
            val.applyResponse(response);
            return;
        }
        Component::onResponse(response, tag);
    }

    NumericVar(IPage& owner, const Literal& name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Variable, id)
        , val{*this, attr::Id::Val}
    {}
};

/** NIS `va` при sta=String: `txt`. Тот же `type`=52, что у NumericVariable. */
template<uint16_t TxtMaxL = 16u>
class StringVar : public Component {
public:
    /** user: значение глобальной строковой переменной */
    attr::String<TxtMaxL> txt;

    void onResponse(const msg::getString& response, uint8_t tag) override
    {
        if (tag == static_cast<uint8_t>(attr::Id::Txt)) {
            txt.applyResponse(response);
            return;
        }
        Component::onResponse(response, tag);
    }

    void onResponse(const msg::getNumeric& response, uint8_t tag) override
    {
        Component::onResponse(response, tag);
    }

    StringVar(IPage& owner, const Literal& name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Variable, id)
        , txt{*this, attr::Id::Txt}
    {}
};

// --- Геометрия и визуальная оболочка (листья; базы — nexCompImpl.hpp) --------

class Hotspot : public TouchArea {
public:
    Hotspot(IPage& owner, const Literal& name, uint8_t id = 0)
        : TouchArea(owner, name, Component::Type::Hotspot, id) {}
};

/** pco; dis; txt (данные QR) */

class QRCode : public Styled<BG::Color> {
public:
    void setPenColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Pco, v);
    }

    //TODO проверить реальное назначение атрибута dis
    void setDataSpacing(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Dis, v);
    }

    void setText(const char* text) noexcept
    {
        attr_detail::assignText(*this, attr::Id::Txt, text);
    }

    QRCode(IPage& owner, const Literal& name, uint8_t id = 0)
        : Styled<BG::Color>(owner, name, Component::Type::QRCode, id)
    {}
};

/** NIS type 112: `pic`, без `sta` как у Styled. */
class Picture : public Drawable {
public:
    void setImage(PicId v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Pic, v);
    }

    Picture(IPage& owner, const Literal& name, uint8_t id = 0)
        : Drawable(owner, name, Component::Type::Picture, id)
    {}
};

/** NIS type 113: `picc` (fullscreen source), без `sta` как у Styled. */
class CropPicture : public Drawable {
public:
    void setCrop(PicId v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Picc, v);
    }

    CropPicture(IPage& owner, const Literal& name, uint8_t id = 0)
        : Drawable(owner, name, Component::Type::CropPicture, id)
    {}
};

template<BG S = BG::Color, uint8_t ChannelCount = 1u>
class Waveform : public Drawable {
public:
    static constexpr BG kStyle = S;

    /** mcu: фон (`bco`/`pic`/`picc`, `gdc`/`gdw`/`gdh` по `S`) — поля внутри `wfBackground` */
    resources::WfBackground<S> bg;

    /** mcu: каналы (`pco0`…`pco3`, `add`, `addt`); `ChannelCount` задаётся шаблоном (1…4). */
    resources::WaveformChannels<ChannelCount> ch;

    static constexpr uint16_t kDataScaleMin = 10u;
    static constexpr uint16_t kDataScaleMax = 1000u;

    void setDataScale(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Dis,
            clamp(v, kDataScaleMin, kDataScaleMax));
    }

    Waveform(IPage& owner, const Literal& name, uint8_t id = 0)
        : Drawable(owner, name, Component::Type::Waveform, id)
        , bg{*this}
        , ch{*this}
    {}
};

template<BG S = BG::Color>
class ProgressBar : public Drawable {
    static_assert(S == BG::Color || S == BG::Image,
        "ProgressBar: only BGStyle::Color and BGStyle::Image are supported");

public:
    static constexpr BG kStyle = S;

    /** mcu: уровень заливки */
    attr::Num<uint8_t> value;

    /** mcu: фон (`bco` / `bpic`) — поля внутри `bg` */
    resources::PbBackground<S> bg;

    /** mcu: заливка (`pco`/`dis` / `ppic`) — поля внутри `bar` */
    resources::PbBar<S> bar;

    void onResponse(const msg::getNumeric& response, uint8_t tag) override
    {
        if (tag == static_cast<uint8_t>(attr::Id::Val)) {
            value.applyResponse(response);
            return;
        }
        TouchArea::onResponse(response, tag);
    }

    ProgressBar(IPage& owner, const Literal& name, uint8_t id = 0)
        : Drawable(owner, name, Component::Type::ProgressBar, id)
        , value{*this, attr::Id::Val}
        , bg{*this}
        , bar{*this}
    {}
};

template<BG S = BG::Color>
class Slider : public Styled<S> {
    static_assert(S != BG::Transparent,
        "Slider: BGStyle::Transparent is not supported");

public:
    /** user: положение ползунка */
    attr::Num<uint8_t> value;

    /** mcu: бегунок — поля в `cursor` */
    resources::Cursor cursor;
    /** mcu: bco1 — поля в `bg2` */
    resources::Background<S, 1u> bg2;

    void onResponse(const msg::getNumeric& response, uint8_t tag) override
    {
        if (tag == static_cast<uint8_t>(attr::Id::Val)) {
            value.applyResponse(response);
            return;
        }
        TouchArea::onResponse(response, tag);
    }

    Slider(IPage& owner, const Literal& name, uint8_t id = 0)
        : Styled<S>(owner, name, Component::Type::Slider, id)
        , value{*this, attr::Id::Val}
        , cursor{*this}
        , bg2{*this}
    {}
};

template<BG S = BG::Color>
class Gauge : public Styled<S> {
public:
    static constexpr uint16_t kAngleMin = 0;
    static constexpr uint16_t kAngleMax = 360;

    /** mcu: центр шкалы — поля внутри `center` */
    resources::GaugeCenter center;

    /** mcu: стрелка — поля внутри `pointer` */
    resources::GaugePointer pointer;

    void setAngle(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Val,
            clamp(v, kAngleMin, kAngleMax));
    }

    Gauge(IPage& owner, const Literal& name, uint8_t id = 0)
        : Styled<S>(owner, name, Component::Type::Gauge, id)
        , center{*this}
        , pointer{*this}
    {}
};

template<BG S = BG::Color>
class ComboBox : public ListSelect<S> {
public:
    void setVAlign(VAlign v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Ycen, v);
    }

    void setHAlign(HAlign v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Xcen, v);
    }

    /** mcu: рамка — поля внутри `border` */
    resources::ComboBorder border;

    /** mcu: стрелка — поля внутри `arrow` */
    resources::ComboArrow arrow;

    /** mcu: ячейки списка — поля внутри `cells` */
    resources::ComboCells cells;

    /** user: список раскрыт (NIS `down`) */
    attr::Num<bool> isOpened;

    void onResponse(const msg::getNumeric& response, uint8_t tag) override
    {
        if (tag == static_cast<uint8_t>(attr::Id::Down)) {
            isOpened.applyResponse(response);
            return;
        }
        ListSelect<S>::onResponse(response, tag);
    }

    ComboBox(IPage& owner, const Literal& name, uint8_t id = 0)
        : ListSelect<S>(owner, name, Component::Type::ComboBox, id)
        , border{*this}
        , arrow{*this}
        , cells{*this}
        , isOpened{*this, attr::Id::Down}
    {}
};

template<BG S = BG::Color>
class TextSelect : public ListSelect<S> {
public:
    void setSelColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Pco2, v);
    }

    void setLineColor(nex::Color v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Pco1, v);
    }

    void setSelectionLine(bool enabled) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Dis, enabled);
    }

    // NIS `txt` (RO) — в MCU API не зеркалим: строки из path знает приложение; выбор — val (ListSelect).

    TextSelect(IPage& owner, const Literal& name, uint8_t id = 0)
        : ListSelect<S>(owner, name, Component::Type::TextSelect, id)
    {}
};

template<BG S = BG::Color, uint16_t TxtMaxL = 16u>
class Text : public Textual<S> {
public:
    void setVAlign(VAlign v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Ycen, v);
    }

    /** user: текст (ввод с клавиатуры) */
    attr::String<TxtMaxL> txt;

    void enablePassword() noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Pw, true);
    }

    void disablePassword() noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Pw, false);
    }

    void onResponse(const msg::getString& response, uint8_t tag) override
    {
        if (tag == static_cast<uint8_t>(attr::Id::Txt)) {
            txt.applyResponse(response);
            return;
        }
        TouchArea::onResponse(response, tag);
    }

    void onResponse(const msg::getNumeric& response, uint8_t tag) override
    {
        TouchArea::onResponse(response, tag);
    }

    Text(IPage& owner, const Literal& name, uint8_t id = 0)
        : Textual<S>(owner, name, Component::Type::Text, id)
        , txt{*this, attr::Id::Txt}
    {}
};

template<BG S = BG::Color, uint16_t TxtMaxL = 16u>
class SlidingText : public Textual<S> {
public:
    /** user: текст */
    attr::String<TxtMaxL> txt;

    // NIS type 62: нет `ycen` — запрет assign через SlidingText&.
    // `ch`, `maxval_y` в NIS есть, но в MCU API намеренно не экспортируем (Editor / val_y достаточно).
    void setVAlign(VAlign) = delete;

    void setShowProgressBar(ShowProgressBar v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Left, static_cast<uint8_t>(v));
    }

    /** user: позиция прокрутки по Y */
    attr::Num<Coord> val_y;

    void onResponse(const msg::getString& response, uint8_t tag) override
    {
        if (tag == static_cast<uint8_t>(attr::Id::Txt)) {
            txt.applyResponse(response);
            return;
        }
        TouchArea::onResponse(response, tag);
    }

    void onResponse(const msg::getNumeric& response, uint8_t tag) override
    {
        if (tag == static_cast<uint8_t>(attr::Id::ValY)) {
            val_y.applyResponse(response);
            return;
        }
        Textual<S>::onResponse(response, tag);
    }

    SlidingText(IPage& owner, const Literal& name, uint8_t id = 0)
        : Textual<S>(owner, name, Component::Type::SlidingText, id)
        , txt{*this, attr::Id::Txt}
        , val_y{*this, attr::Id::ValY}
    {}
};

template<BG S = BG::Color, uint16_t TxtMaxL = 16u>
class ScrollText : public Textual<S> {
public:
    void setVAlign(VAlign v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Ycen, v);
    }

    /** user: текст */
    attr::String<TxtMaxL> txt;

    static constexpr uint16_t kPeriodMin = 80u;
    static constexpr uint16_t kPeriodMax = 65535u;
    static constexpr uint8_t kScrollStepMin = 2u;
    static constexpr uint8_t kScrollStepMax = 50u;

    void setScrollDirection(ScrollDirection v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Dir, static_cast<uint8_t>(v));
    }

    void setScrollStep(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Dis,
            clamp(v, kScrollStepMin, kScrollStepMax));
    }

    void setPeriod(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Tim,
            clamp(v, kPeriodMin, kPeriodMax));
    }

    void enable() noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::En, true);
    }

    void disable() noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::En, false);
    }

    void onResponse(const msg::getString& response, uint8_t tag) override
    {
        if (tag == static_cast<uint8_t>(attr::Id::Txt)) {
            txt.applyResponse(response);
            return;
        }
        TouchArea::onResponse(response, tag);
    }

    ScrollText(IPage& owner, const Literal& name, uint8_t id = 0)
        : Textual<S>(owner, name, Component::Type::ScrollText, id)
        , txt{*this, attr::Id::Txt}
    {}
};

template<BG S = BG::Color>
class Button : public ButtonBase<S> {
public:
    Button(IPage& owner, const Literal& name, uint8_t id = 0)
        : ButtonBase<S>(owner, name, Component::Type::Button, id)
    {}
};

template<BG S = BG::Color>
class DualStateButton : public ButtonBase<S> {
public:
    /** user: состояние кнопки (нажато/отпущено) */
    attr::Num<uint32_t> val;

    void onResponse(const msg::getNumeric& response, uint8_t tag) override
    {
        if (tag == static_cast<uint8_t>(attr::Id::Val)) {
            val.applyResponse(response);
            return;
        }
        ButtonBase<S>::onResponse(response, tag);
    }

    DualStateButton(IPage& owner, const Literal& name, uint8_t id = 0)
        : ButtonBase<S>(owner, name, Component::Type::DualStateButton, id)
        , val{*this, attr::Id::Val}
    {}
};

template<BG S = BG::Color>
class Number : public Numeric<S> {
public:
    void setDigitCount(uint8_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Lenth, v);
    }

    void setFormat(NumFormat v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Format, v);
    }

    Number(IPage& owner, const Literal& name, uint8_t id = 0)
        : Numeric<S>(owner, name, Component::Type::Number, id)
    {}
};

template<BG S = BG::Color>
class XFloat : public Numeric<S> {
public:
    /** mcu: формат числа — знаки до и после точки (NIS `vvs0`, `vvs1`) */
    void setFormat(uint8_t digitsBeforePoint, uint8_t digitsAfterPoint) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Vvs0, digitsBeforePoint);
        attr_detail::assignNumeric(*this, attr::Id::Vvs1, digitsAfterPoint);
    }

    XFloat(IPage& owner, const Literal& name, uint8_t id = 0)
        : Numeric<S>(owner, name, Component::Type::XFloat, id)
    {}
};

class Checkbox : public Selection {
public:
    Checkbox(IPage& owner, const Literal& name, uint8_t id = 0)
        : Selection(owner, name, Component::Type::Checkbox, id) {}
};

class Radio : public Selection {
public:
    Radio(IPage& owner, const Literal& name, uint8_t id = 0)
        : Selection(owner, name, Component::Type::Radio, id) {}
};

class ToggleSwitch : public Selection {
public:
    /** mcu: состояние «вкл» — `pressed.bg` → `bco2`, `pressed.setMarkerColor` → `pco2` */
    resources::PressedMarker pressed;

    /** mcu: шрифт подписи (`font`, `pco1`) — поля внутри `font` */
    resources::FontId<1u> font;

    void setLabelGap(uint16_t v) noexcept
    {
        attr_detail::assignNumeric(*this, attr::Id::Dis, v);
    }

    /** mcu: подпись переключателя */
    //todo: сделать setter с двумя const char* которые склеиваются в одну строку через "/" - неясно? нужен временный буффер???
    //возможно использовать txt как private поле
    attr::String<24> txt;

    ToggleSwitch(IPage& owner, const Literal& name, uint8_t id = 0)
        : Selection(owner, name, Component::Type::ToggleSwitch, id)
        , pressed{*this}
        , font{*this}
        , txt{*this, attr::Id::Txt}
    {}
};


} // namespace comp
} // namespace nex
