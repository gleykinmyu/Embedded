#pragma once

#include "../comp/nexCanvas.hpp"
#include "../core/nexCommands.hpp"
#include "nexSysVars.hpp"

namespace nex {

class Application;

/** Режим сна и таймеры/пробуждение (NIS §6.7–6.12) — поле `Application::sleep`. */
class AppSleep {
    friend class Application;
    explicit AppSleep(Application& a) noexcept;

    Application& _app;

public:
    /** Войти в sleep (`sleep=1`). */
    void enter() const noexcept;
    /** Выйти из sleep (`sleep=0`). */
    void exit() const noexcept;
    /** Сон при отсутствии UART, сек (`ussp`, 3…65535). */
    void noSerialTimer(uint16_t seconds) const noexcept;
    /** Сон при отсутствии касаний, сек (`thsp`, 3…65535). */
    void noTouchTimer(uint16_t seconds) const noexcept;
    /** Пробуждение по касанию (`thup`). */
    void wakeOnTouch(bool wake) const noexcept;
    /** Страница при выходе из sleep (`wup`; 255 — текущая, только refresh). */
    void wakeUpPage(uint8_t pageId) const noexcept;
    /** Пробуждение по данным UART (`usup`). */
    void wakeOnSerial(bool wake) const noexcept;

};

/** Системные переменные касания — поле `Application::touch`. */
class AppTouch {
    friend class Application;
    explicit AppTouch(Application& a) noexcept;

    Application& _app;

public:
    /** Вкл./выкл. отчёт координат **0x67** / **0x68** (`sendxy`, NIS §6.10). */
    void sendXY(bool enable) const noexcept;
    /** `tsw 255,…` — вкл./выкл. touch на всех компонентах (NIS). */
    void setAllTouchable(bool on) const noexcept;

    SysVar<int16_t> tch[4];

    /** Калибровка touch (`cali`). */
    void calibrate() const noexcept;
};

} // namespace nex
