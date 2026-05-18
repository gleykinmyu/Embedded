#pragma once

#include <array>
#include <cstdint>
#include "../core/nexTypes.hpp"
#include "nexComponentBase.hpp"

namespace nex {
// --- Прямые наследники Component (без x,y,w,h на странице) --------------------

/** tim, en */
class Timer : public Component {
public:
    uint32_t tim{}; /**< Интервал, мс (`tim`). */
    bool en{};      /**< Включён (`en`). */

    Timer(Page& owner, const Literal& name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Timer, id) {}
};

/** NIS `va` при sta=Number: `val` (32-bit signed). Тот же `type`=52, что у StringVariable. */
class NumericVariable : public Component {
public:
    int32_t val{};

    NumericVariable(Page& owner, const Literal& name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Variable, id) {}
};

/** NIS `va` при sta=String: `txt`, `txt_maxl`. Тот же `type`=52, что у NumericVariable. */
class StringVariable : public Component {
public:
    uint16_t txt_maxl{};
    std::array<char, 256> txt{};

    StringVariable(Page& owner, const Literal& name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Variable, id) {}
};

// --- Геометрия и визуальная оболочка ----------------------------------------

/** `pos` (x, y), `w`, `h` (NIS). */
class GeometryComponent : public Component {
public:
    Point pos{};
    uint16_t w{};
    uint16_t h{};

protected:
    explicit GeometryComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : Component(owner, objectName, componentType, id) {}
};

/** только геометрия; отдельных атрибутов в дельте к Geometry — нет */
class Hotspot : public GeometryComponent {
public:
    Hotspot(Page& owner, const Literal& name, uint8_t id = 0)
        : GeometryComponent(owner, name, Component::Type::Hotspot, id) {}
};

/** drag, aph, effect (NIS) */
class VisualComponent : public GeometryComponent {
public:
    bool drag{};
    uint8_t aph{};    /**< Непрозрачность 0…127 (`aph`). */
    uint8_t effect{}; /**< Код эффекта (`effect`), набор значений по серии. */

protected:
    explicit VisualComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : GeometryComponent(owner, objectName, componentType, id) {}
};

/**
 * Фон (`bco`, `pic`, `picc`) в зависимости от `Style` как параметра шаблона (compile-time `sta`).
 * Значение для UART: `static_cast<uint8_t>(style())` при совпадении с NIS по серии.
 */
template<BGStyle S>
class BGComponent : public VisualComponent {
public:
    static constexpr BGStyle kStyle = S;

    static constexpr BGStyle style() noexcept { return S; }

    Color bco{};   /**< Фон / цвет заливки при соответствующем `sta`. */
    PicId pic{};   /**< Фоновая картинка (`pic` / bpic и т.п. по типу). */
    PicId picc{};  /**< Картинка-источник crop (`picc`). */

protected:
    explicit BGComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : VisualComponent(owner, objectName, componentType, id) {}
};

/** pco; dis; txt; txt_maxl (данные QR) */
class QRCode : public BGComponent<BGStyle::Color> {
public:
    Color pco{}; /**< Цвет модулей QR (`pco`). */
    uint16_t dis{};
    std::array<char, 256> txt{};
    uint16_t txt_maxl{};

    QRCode(Page& owner, const Literal& name, uint8_t id = 0)
        : BGComponent<BGStyle::Color>(owner, name, Component::Type::QRCode, id) {}
};

/** pic — поле `pic` в BG; `Style::Image`. */
class Picture : public BGComponent<BGStyle::Image> {
public:
    Picture(Page& owner, const Literal& name, uint8_t id = 0)
        : BGComponent<BGStyle::Image>(owner, name, Component::Type::Picture, id) {}
};

/** cpic — `Style::CropImage`. */
class CropPicture : public BGComponent<BGStyle::CropImage> {
public:
    PicId cpic{}; /**< Окно кропа по ресурсу (`cpic`). */

    CropPicture(Page& owner, const Literal& name, uint8_t id = 0)
        : BGComponent<BGStyle::CropImage>(owner, name, Component::Type::CropPicture, id) {}
};

/** Общая ветка drawable + цветовые каналы (дельта к BG — у листьев). */
template<BGStyle S = BGStyle::Color>
class DrawableColoredComponent : public BGComponent<S> {
protected:
    explicit DrawableColoredComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : BGComponent<S>(owner, objectName, componentType, id) {}
};

/** Waveform: ch, gdc, gdw, gdh, pco0…3, dis, wid, hig */
template<BGStyle S = BGStyle::Color>
class Waveform : public DrawableColoredComponent<S> {
public:
    uint8_t ch{};
    Color pco0{}, pco1{}, pco2{}, pco3{};
    uint16_t gdc{};
    uint16_t gdw{};
    uint16_t gdh{};
    uint8_t dis{}; /**< Маска отключения каналов (`dis`). */
    uint16_t wid{};
    uint16_t hig{};

    Waveform(Page& owner, const Literal& name, uint8_t id = 0)
        : DrawableColoredComponent<S>(owner, name, Component::Type::Waveform, id) {}
};

/**
 * ProgressBar: val; dis (при fill=color); фон `bco`/`bpic`, передний план полосы `pco`/`ppic`
 * (цветовой и картинный режимы — `pb_sta`). У основного `sta` в NIS допустимы только Color и Image.
 */
template<BGStyle S = BGStyle::Color>
class ProgressBar : public DrawableColoredComponent<S> {
public:
    uint32_t val{};
    uint16_t dis{};
    Color pco{}; /**< Цвет заполнения полосы при `pb_sta == Color`. */
    PicId bpic{}; /**< Фон-бар по картинке (`bpic`). */
    PicId ppic{}; /**< Заполнение по картинке (`ppic`). */

    ProgressBar(Page& owner, const Literal& name, uint8_t id = 0)
        : DrawableColoredComponent<S>(owner, name, Component::Type::ProgressBar, id) {}
};

/** Slider: wid, hig, bco1(pic1,picc1), pco, val, maxval, minval, ch */
template<BGStyle S = BGStyle::Color>
class Slider : public DrawableColoredComponent<S> {
public:
    uint16_t wid{};
    uint16_t hig{};
    PicId pic1{};
    PicId picc1{};
    Color pco{};
    uint32_t val{};
    uint16_t maxval{65535};
    uint16_t minval{};
    uint8_t ch{};

    Slider(Page& owner, const Literal& name, uint8_t id = 0)
        : DrawableColoredComponent<S>(owner, name, Component::Type::Slider, id) {}
};

/** Gauge: val, format, up, down, left, pco, pco2, hig, vvs0…2, bco(picc,pic) */
template<BGStyle S = BGStyle::Color>
class Gauge : public DrawableColoredComponent<S> {
public:
    int32_t val{};
    std::array<char, 32> format{};
    PicId pic_up{};
    PicId pic_down{};
    PicId pic_left{};
    Color pco{};
    Color pco2{};
    uint16_t hig{};
    std::array<char, 24> vvs0{};
    std::array<char, 24> vvs1{};
    std::array<char, 24> vvs2{};

    Gauge(Page& owner, const Literal& name, uint8_t id = 0)
        : DrawableColoredComponent<S>(owner, name, Component::Type::Gauge, id) {}
};

/** font; pco; spax */
template<BGStyle S = BGStyle::Color>
class PrintableComponent : public BGComponent<S> {
public:
    FontId font{};
    Color pco{};   /**< Базовый цвет текста / глифов (`pco`). */
    int16_t spax{}; /**< Межсимвольный интервал (`spax`). */

protected:
    explicit PrintableComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : BGComponent<S>(owner, objectName, componentType, id) {}
};

/**
 * DataFileRecordComponent — поля таблицы/файлов (DataRecord, FileBrowser);
 * RO в NIS помечены в комментариях.
 */
template<BGStyle S = BGStyle::Color>
class DataFileRecordComponent : public PrintableComponent<S> {
public:
    std::array<char, 256> txt{};
    uint16_t txt_maxl{};
    uint16_t left{};
    uint8_t ch{};
    uint8_t dir{};
    int32_t val{};
    uint16_t qty{}; /**< Копия/кэш `qty` (в панели часто RO). */
    uint16_t dis{};
    uint16_t maxval_y{};
    uint16_t maxval_x{};
    uint16_t val_x{};
    uint16_t val_y{};
    Color bco2{};
    Color pco2{};

protected:
    explicit DataFileRecordComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : PrintableComponent<S>(owner, objectName, componentType, id) {}
};

/** path; path_m(RO); val; ch; dis; hig */
template<BGStyle S = BGStyle::Color>
class ListSelectTextComponent : public PrintableComponent<S> {
public:
    std::array<char, 512> path{};
    uint16_t path_m{}; /**< RO в редакторе — при необходимости обновлять из дисплея. */
    int32_t val{};
    uint8_t ch{};
    uint16_t dis{};
    uint16_t hig{};

protected:
    explicit ListSelectTextComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : PrintableComponent<S>(owner, objectName, componentType, id) {}
};

/** ComboBox — дельта к ListSelectTextComponent (NIS). */
template<BGStyle S = BGStyle::Color>
class ComboBox : public ListSelectTextComponent<S> {
public:
    bool ycen{};
    PicId pic_up_attr{};   /**< `up` — id ресурса стрелки/иконки. */
    Color pco3{};
    Color bco1{};
    Color pco1{};
    uint8_t list_dir{}; /**< `dir` — код направления списка (NIS по серии). */
    uint16_t qty{};
    int16_t vvs0{};
    Color bco2{};
    Color pco2{};
    PicId pic_down_attr{}; /**< `down`. */
    uint8_t mode{};
    uint16_t wid{};
    int16_t vvs1{};
    bool xcen{};

    std::array<char, 256> txt{};
    uint16_t txt_maxl{};

    ComboBox(Page& owner, const Literal& name, uint8_t id = 0)
        : ListSelectTextComponent<S>(owner, name, Component::Type::ComboBox, id) {}
};

/** spay; isbr; ycen; xcen */
template<BGStyle S = BGStyle::Color>
class MultilineComponent : public PrintableComponent<S> {
public:
    int16_t spay{};
    bool isbr{};
    bool ycen{};
    bool xcen{};

protected:
    explicit MultilineComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : PrintableComponent<S>(owner, objectName, componentType, id) {}
};

/** txt; txt_maxl */
template<BGStyle S = BGStyle::Color>
class TextComponent : public MultilineComponent<S> {
public:
    std::array<char, 256> txt{};
    uint16_t txt_maxl{};

protected:
    explicit TextComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : MultilineComponent<S>(owner, objectName, componentType, id) {}
};

/** key; pw */
template<BGStyle S = BGStyle::Color>
class Text : public TextComponent<S> {
public:
    bool key{};
    bool pw{}; /**< Маска пароля (`pw`). */

    Text(Page& owner, const Literal& name, uint8_t id = 0)
        : TextComponent<S>(owner, name, Component::Type::Text, id) {}
};

/** key; dir; dis; tim; en */
template<BGStyle S = BGStyle::Color>
class ScrollText : public TextComponent<S> {
public:
    bool key{};
    uint8_t dir{}; /**< Направление прокрутки (`dir`). */
    uint16_t dis{};
    uint32_t tim{};
    bool en{};

    ScrollText(Page& owner, const Literal& name, uint8_t id = 0)
        : TextComponent<S>(owner, name, Component::Type::ScrollText, id) {}
};

/** bco2(pic2,picc2); pco2 */
template<BGStyle S = BGStyle::Color>
class ButtonLikeComponent : public TextComponent<S> {
public:
    Color bco2{};
    PicId pic2{};
    PicId picc2{};
    Color pco2{};

protected:
    explicit ButtonLikeComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : TextComponent<S>(owner, objectName, componentType, id) {}
};

template<BGStyle S = BGStyle::Color>
class Button : public ButtonLikeComponent<S> {
public:
    Button(Page& owner, const Literal& name, uint8_t id = 0)
        : ButtonLikeComponent<S>(owner, name, Component::Type::Button, id) {}
};

/** val — состояние dual-state */
template<BGStyle S = BGStyle::Color>
class DualStateButton : public ButtonLikeComponent<S> {
public:
    uint32_t val{}; /**< 0/1 или диапазон по проекту (`val`). */

    DualStateButton(Page& owner, const Literal& name, uint8_t id = 0)
        : ButtonLikeComponent<S>(owner, name, Component::Type::DualStateButton, id) {}
};

/** key; val; format */
template<BGStyle S = BGStyle::Color>
class NumericComponent : public MultilineComponent<S> {
public:
    bool key{};
    int32_t val{};
    std::array<char, 32> format{}; /**< Строка формата (`format`). */

protected:
    explicit NumericComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : MultilineComponent<S>(owner, objectName, componentType, id) {}
};

/** length (`spay` — у MultilineComponent / Numeric). */
template<BGStyle S = BGStyle::Color>
class Number : public NumericComponent<S> {
public:
    uint16_t length{}; /**< Лимит символов (`length`). */

    Number(Page& owner, const Literal& name, uint8_t id = 0)
        : NumericComponent<S>(owner, name, Component::Type::Number, id) {}
};

/** vvs0; vvs1 (`spay` — у MultilineComponent). */
template<BGStyle S = BGStyle::Color>
class XFloat : public NumericComponent<S> {
public:
    std::array<char, 24> vvs0{};
    std::array<char, 24> vvs1{};

    XFloat(Page& owner, const Literal& name, uint8_t id = 0)
        : NumericComponent<S>(owner, name, Component::Type::XFloat, id) {}
};

/**
 * pco, val; фон — `bco` (BGComponent). В NIS у Checkbox/Radio нет отдельного `sta`/font;
 * `Style` задаётся шаблоном для единообразия с BG-веткой (по умолчанию Color).
 */
template<BGStyle S = BGStyle::Color>
class SelectionComponent : public BGComponent<S> {
public:
    Color pco{};
    uint32_t val{}; /**< Состояние/индекс выбора (зависит от типа). */

protected:
    explicit SelectionComponent(Page& owner, const Literal& objectName, Component::Type componentType, uint8_t id = 0) noexcept
        : BGComponent<S>(owner, objectName, componentType, id) {}
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

/** ToggleSwitch — дельта к Selection (NIS). */
template<BGStyle S = BGStyle::Color>
class ToggleSwitch : public SelectionComponent<S> {
public:
    Color bco2{};
    Color pco2{};
    Color pco1{}; /**< «Цвет шрифта» в панели (`pco1`). */
    FontId font{};
    uint16_t dis{};
    std::array<char, 25> txt{}; /**< txt_maxl = 24 в NIS + NUL. */

    ToggleSwitch(Page& owner, const Literal& name, uint8_t id = 0)
        : SelectionComponent<S>(owner, name, Component::Type::ToggleSwitch, id) {}
};

} // namespace nex
