#pragma once

#include "../core/nexCommands.hpp"
#include "nexSysVars.hpp"

namespace nex {

class Application;

/** Фасад `cmd::Eeprom` — поле `Application::ep`; создаётся только из `Application`. */
class AppEeprom {
    friend class Application;

public:
    void write(const AttrRef& variableOrConstant, uint32_t eepromStart) const noexcept;
    void read(const AttrRef& destVariable, uint32_t eepromStart) const noexcept;
    /** `wept` / `rept`: преамбула + transparent (`Transaction::State::AwaitingTransparentTx` / `AwaitingRawDataRx`). */
    void write_t(uint32_t eepromStart, const uint8_t* buffer, uint32_t byteCount) const noexcept;
    void read_t(uint32_t eepromStart, uint8_t* buffer, uint32_t byteCount) const noexcept;

private:
    Application& _app;
    explicit AppEeprom(Application& a) noexcept;
};

/** Фасад `cmd::File` и `cmd::Directory` — поле `Application::fs`. */
class AppFileSystem {
    friend class Application;

public:
    void file_remove(const char* pathQuoted) const noexcept;
    void file_rename(const char* pathFromQuoted, const char* pathToQuoted) const noexcept;
    void file_find(const char* pathQuoted, const AttrRef& dstNumAttr) const noexcept;
    void file_create(const char* pathQuoted, uint32_t reservedSize) const noexcept;
    /** `rdfile`: преамбула + приём transparent (`Transaction::State::AwaitingRawDataRx`). */
    void file_read_t(const char* pathQuoted, uint32_t offset, uint8_t* buffer, uint32_t byteCount, uint32_t crcOption) const noexcept;
    /** `twfile`: преамбула + передача transparent (`Transaction::State::AwaitingTransparentTx`). */
    void file_write_t(const char* pathQuoted, const uint8_t* buffer, uint32_t byteCount) const noexcept;

    void dir_remove(const char* pathQuoted) const noexcept;
    void dir_create(const char* pathQuoted) const noexcept;
    void dir_rename(const char* pathFromQuoted, const char* pathToQuoted) const noexcept;
    void dir_find(const char* pathQuoted, const AttrRef& dstNumAttr) const noexcept;

private:
    Application& _app;
    explicit AppFileSystem(Application& a) noexcept;
};

/** Фасад `cmd::gui::*` и связанные системные переменные — поле `Application::cs`. */
class AppCanvas {
    friend class Application;

    Application& _app;

public:
    /** Межсимвольный интервал текста (`spax`). */
    void setCharSpacing(uint16_t spacing) const noexcept;
    /** Межстрочный интервал текста (`spay`). */
    void setLineSpacing(uint16_t spacing) const noexcept;
    /** Цвет кисти touch-рисования (`thc`). */
    void setTouchDrawColor(Color color) const noexcept;
    /** Вкл./выкл. touch-рисование (`thdra`). */
    void drawOnTouch(bool enable) const noexcept;

    void clear_screen(Color color) const noexcept;
    void picture(Point at, PicId pictureId) const noexcept;
    void picture_in_place(Point upperLeft, uint32_t w, uint32_t h, PicId pictureId) const noexcept;
    void picture_draw(Point dst, uint32_t w, uint32_t h, Point src, PicId pictureId) const noexcept;
    void text_in_region(Point upperLeft, uint32_t w, uint32_t h, FontId fontId, Color fg, Color bg,
        uint32_t hAlign, uint32_t vAlign, BGStyle fill, const char* contentToken) const noexcept;
    void rect_fill(Point upperLeft, Point lowerRightInclusive, Color color) const noexcept;
    void rect_outline(Point upperLeft, Point lowerRightInclusive, Color color) const noexcept;
    void line(Point from, Point to, Color color) const noexcept;
    void circle_outline(Point center, uint32_t radius, Color color) const noexcept;
    void circle_filled(Point center, uint32_t radius, Color color) const noexcept;

    explicit AppCanvas(Application& a) noexcept;
};

/** Один аудиоканал (`play`, `audioN`: stop/pause/resume). */
class AppAudioChannel {
    friend class AppAudio;

    Application& _app;
    uint8_t _id;

    AppAudioChannel(Application& a, uint8_t id) noexcept;

public:
    void play(uint32_t resourceId, uint32_t loop01) const noexcept;
    void stop() const noexcept;
    void pause() const noexcept;
    void resume() const noexcept;
};

/** EQ, громкость и каналы — поле `Application::audio`. */
class AppAudio {
    friend class Application;

    Application& _app;

public:
    void setEqLow(uint8_t level) const noexcept;
    void setEqMid(uint8_t level) const noexcept;
    void setEqHigh(uint8_t level) const noexcept;
    /** Полоса EQ `eq0`…`eq9` (`band` 0…9). */
    void setEqBand(uint8_t band, uint8_t level) const noexcept;
    void setVolume(uint8_t level) const noexcept;

    AppAudioChannel ch[2];

    explicit AppAudio(Application& a) noexcept;
};

/** Режим сна и таймеры/пробуждение (NIS §6.7–6.12) — поле `Application::sleep`. */
class AppSleep {
    friend class Application;

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

    explicit AppSleep(Application& a) noexcept;
};

/** Системные переменные касания — поле `Application::touch`. */
class AppTouch {
    friend class Application;

    Application& _app;

public:
    /** Вкл./выкл. отчёт координат **0x67** / **0x68** (`sendxy`, NIS §6.10). */
    void sendXY(bool enable) const noexcept;
    /** `tsw 255,…` — touch для всех компонентов (NIS). */
    void touchSwitch(bool enabled) const noexcept;

    SysVar<int16_t> tch[4];

    void calibrate() const noexcept;

    explicit AppTouch(Application& a) noexcept;
};

} // namespace nex
