#pragma once

#include "nexComponents.hpp"

namespace nex {

/**
 * Текст (префикс в редакторе `t`).
 * Специфичные атрибуты (NIS): txt, txt_maxl, font, pco, bco, xcen, ycen, spax, spay, length, pw, wid — RW
 * (лимит буфера: txt_maxl — ITEAD User Guide / Editor txt-maxl; length — встречается в NIS по сериям — Attribute Pane)
 */
class Text : public Component {
public:
    Text(Page& owner, const char* name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Text, id) {}
};

/**
 * Бегущая строка.
 * Специфичные атрибуты (NIS): txt, txt_maxl, spax, spdy, font, pco, bco — RW
 * (txt_maxl — как у Text; Editor Guide: встроенный таймер, без .pw)
 */
class ScrollText : public Component {
public:
    ScrollText(Page& owner, const char* name, uint8_t id = 0)
        : Component(owner, name, Component::Type::ScrollText, id) {}
};

/**
 * Число (префикс `n`).
 * Специфичные атрибуты (NIS): val, format, lenth, font, pco, bco — RW
 */
class Number : public Component {
public:
    Number(Page& owner, const char* name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Number, id) {}
};

/**
 * XFloat.
 * Специфичные атрибуты (NIS): val, font, pco, bco, format — RW
 */
class XFloat : public Component {
public:
    XFloat(Page& owner, const char* name, uint8_t id = 0)
        : Component(owner, name, Component::Type::XFloat, id) {}
};

/**
 * Кнопка.
 * Специфичные атрибуты (NIS): txt, txt_maxl, font, pco, bco, pic, pic2, picc — RW
 * (txt_maxl — ITEAD User Guide / Editor)
 */
class Button : public Component {
public:
    Button(Page& owner, const char* name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Button, id) {}
};

/**
 * Dual-state button.
 * Специфичные атрибуты (NIS): txt, txt_maxl, font, pco, bco, pic, pic2, picc, val (состояние) — RW
 */
class DualStateButton : public Component {
public:
    DualStateButton(Page& owner, const char* name, uint8_t id = 0)
        : Component(owner, name, Component::Type::DualStateButton, id) {}
};

/**
 * Картинка.
 * Специфичные атрибуты (NIS): pic, pic2, pco, bco — RW
 */
class Picture : public Component {
public:
    Picture(Page& owner, const char* name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Picture, id) {}
};

/**
 * Crop (обрезка картинки).
 * Специфичные атрибуты (NIS): picc (источник полноэкранного ресурса — Editor Guide); x, y, w, h — окно; pco, bco — RW
 */
class CropPicture : public Component {
public:
    CropPicture(Page& owner, const char* name, uint8_t id = 0)
        : Component(owner, name, Component::Type::CropPicture, id) {}
};

/**
 * Progress bar.
 * Специфичные атрибуты (NIS): val, dir (направление заливки), pic, pco, bco, font — RW
 */
class ProgressBar : public Component {
public:
    ProgressBar(Page& owner, const char* name, uint8_t id = 0)
        : Component(owner, name, Component::Type::ProgressBar, id) {}
};

/**
 * Gauge (стрелочный индикатор).
 * Специфичные атрибуты (NIS): val, needle, pic, pco, bco — RW
 */
class Gauge : public Component {
public:
    Gauge(Page& owner, const char* name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Gauge, id) {}
};

/**
 * Waveform.
 * Специфичные атрибуты (NIS): ch, dis, gch, gdc, pco0…pco3, wid, h — RW (геометрия по серии см. NIS)
 */
class Waveform : public Component {
public:
    Waveform(Page& owner, const char* name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Waveform, id) {}
};

/**
 * Slider.
 * Специфичные атрибуты (NIS): val, minval, maxval, pic, pco, bco — RW
 */
class Slider : public Component {
public:
    Slider(Page& owner, const char* name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Slider, id) {}
};

/**
 * Checkbox.
 * Специфичные атрибуты (NIS): val (0/1), txt, font, pco, bco, pic — RW
 */
class Checkbox : public Component {
public:
    Checkbox(Page& owner, const char* name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Checkbox, id) {}
};

/**
 * Radio.
 * Специфичные атрибуты (NIS): val, txt, font, pco, bco, pic — RW
 */
class Radio : public Component {
public:
    Radio(Page& owner, const char* name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Radio, id) {}
};

/**
 * Switch (в редакторе Nextion).
 * Специфичные атрибуты (NIS): val, pco, bco, pic — RW
 */
class ToggleSwitch : public Component {
public:
    ToggleSwitch(Page& owner, const char* name, uint8_t id = 0)
        : Component(owner, name, Component::Type::ToggleSwitch, id) {}
};

/**
 * Hotspot (невидимая зона касания).
 * Отдельных атрибутов сверх общих (геометрия, en, aph и т.д. — см. Component) обычно нет.
 */
class Hotspot : public Component {
public:
    Hotspot(Page& owner, const char* name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Hotspot, id) {}
};

/**
 * Таймер (компонент на странице).
 * Специфичные атрибуты (NIS): tim (интервал, мс), cyclic — RW
 */
class TimerComponent : public Component {
public:
    TimerComponent(Page& owner, const char* name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Timer, id) {}
};

/**
 * Variable (va …).
 * Специфичные атрибуты (NIS): val или txt (режим int/str), vscope — RW
 * (область видимости имени в проекте: private/global/static; не «видно на экране»).
 */
class Variable : public Component {
public:
    Variable(Page& owner, const char* name, uint8_t id = 0)
        : Component(owner, name, Component::Type::Variable, id) {}
};

/**
 * QR Code.
 * Специфичные атрибуты (NIS): txt (данные QR), txt_maxl, pco, bco — RW
 * (лимит длины по модели — Editor Guide)
 */
class QRCode : public Component {
public:
    QRCode(Page& owner, const char* name, uint8_t id = 0)
        : Component(owner, name, Component::Type::QRCode, id) {}
};

/**
 * Combo box.
 * Специфичные атрибуты (NIS): path (строки вариантов), val, txt, font, pco, bco, pic — RW
 * (path — Editor Guide Intelligent/Edge)
 */
class ComboBox : public Component {
public:
    ComboBox(Page& owner, const char* name, uint8_t id = 0)
        : Component(owner, name, Component::Type::ComboBox, id) {}
};

} // namespace nex
