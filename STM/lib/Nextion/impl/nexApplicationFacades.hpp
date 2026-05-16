#pragma once

#include "../core/nexCommands.hpp"

namespace nex {

class Application;

/** Фасад `cmd::Eeprom` — поле `Application::ep`; создаётся только из `Application`. */
class AppEeprom {
    friend class Application;

public:
    void write(const cmd::TargetAttr& variableOrConstant, uint32_t eepromStart) const noexcept;
    void read(const cmd::TargetAttr& destVariable, uint32_t eepromStart) const noexcept;
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
    void file_find(const char* pathQuoted, const cmd::TargetAttr& dstNumAttr) const noexcept;
    void file_create(const char* pathQuoted, uint32_t reservedSize) const noexcept;
    /** `rdfile`: преамбула + приём transparent (`Transaction::State::AwaitingRawDataRx`). */
    void file_read_t(const char* pathQuoted, uint32_t offset, uint8_t* buffer, uint32_t byteCount, uint32_t crcOption) const noexcept;
    /** `twfile`: преамбула + передача transparent (`Transaction::State::AwaitingTransparentTx`). */
    void file_write_t(const char* pathQuoted, const uint8_t* buffer, uint32_t byteCount) const noexcept;

    void dir_remove(const char* pathQuoted) const noexcept;
    void dir_create(const char* pathQuoted) const noexcept;
    void dir_rename(const char* pathFromQuoted, const char* pathToQuoted) const noexcept;
    void dir_find(const char* pathQuoted, const cmd::TargetAttr& dstNumAttr) const noexcept;

private:
    Application& _app;
    explicit AppFileSystem(Application& a) noexcept;
};

/** Фасад `cmd::gui::*` — поле `Application::cs`. */
class AppCanvas {
    friend class Application;

public:
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

private:
    Application& _app;
    explicit AppCanvas(Application& a) noexcept;
};

} // namespace nex
