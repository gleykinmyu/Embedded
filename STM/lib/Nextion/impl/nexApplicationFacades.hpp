#pragma once

#include "../core/nexCommands.hpp"

namespace nex {

class Application;

/** Фасад `cmd::Eeprom` — поле `Application::ep`; создаётся только из `Application`. */
class AppEeprom {
    friend class Application;

private:
    Application& _app;
    explicit AppEeprom(Application& a) noexcept;

public:
    bool write(const cmd::TargetAttr& variableOrConstant, uint32_t eepromStart) const noexcept;
    bool read(const cmd::TargetAttr& destVariable, uint32_t eepromStart) const noexcept;
    /** `wept` / `rept`: преамбула + transparent через `Application::requestTransparentTransmit` / `requestTransparentReceive`. */
    bool write_t(uint32_t eepromStart, const uint8_t* buffer, uint32_t byteCount) const noexcept;
    bool read_t(uint32_t eepromStart, uint8_t* buffer, uint32_t byteCount) const noexcept;
};

/** Фасад `cmd::File` и `cmd::Directory` — поле `Application::fs`. */
class AppFileSystem {
    friend class Application;

private:
    Application& _app;
    explicit AppFileSystem(Application& a) noexcept;

public:
    bool file_remove(const char* pathQuoted) const noexcept;
    bool file_rename(const char* pathFromQuoted, const char* pathToQuoted) const noexcept;
    bool file_find(const char* pathQuoted, const cmd::TargetAttr& dstNumAttr) const noexcept;
    bool file_create(const char* pathQuoted, uint32_t reservedSize) const noexcept;
    /** `rdfile`: преамбула + приём transparent через `Application::requestTransparentReceive`. */
    bool file_read_t(const char* pathQuoted, uint32_t offset, uint8_t* buffer, uint32_t byteCount, uint32_t crcOption) const noexcept;
    /** `twfile`: преамбула + передача transparent через `Application::requestTransparentTransmit`. */
    bool file_write_t(const char* pathQuoted, const uint8_t* buffer, uint32_t byteCount) const noexcept;

    bool dir_remove(const char* pathQuoted) const noexcept;
    bool dir_create(const char* pathQuoted) const noexcept;
    bool dir_rename(const char* pathFromQuoted, const char* pathToQuoted) const noexcept;
    bool dir_find(const char* pathQuoted, const cmd::TargetAttr& dstNumAttr) const noexcept;
};

/** Фасад `cmd::gui::*` — поле `Application::cs`. */
class AppCanvas {
    friend class Application;

private:
    Application& _app;
    explicit AppCanvas(Application& a) noexcept;

public:
    bool clear_screen(Color color) const noexcept;
    bool picture(Point at, PicId pictureId) const noexcept;
    bool picture_in_place(Point upperLeft, uint32_t w, uint32_t h, PicId pictureId) const noexcept;
    bool picture_draw(Point dst, uint32_t w, uint32_t h, Point src, PicId pictureId) const noexcept;
    bool text_in_region(Point upperLeft, uint32_t w, uint32_t h, FontId fontId, Color fg, Color bg,
        uint32_t hAlign, uint32_t vAlign, BGStyle fill, const char* contentToken) const noexcept;
    bool rect_fill(Point upperLeft, Point lowerRightInclusive, Color color) const noexcept;
    bool rect_outline(Point upperLeft, Point lowerRightInclusive, Color color) const noexcept;
    bool line(Point from, Point to, Color color) const noexcept;
    bool circle_outline(Point center, uint32_t radius, Color color) const noexcept;
    bool circle_filled(Point center, uint32_t radius, Color color) const noexcept;
};

} // namespace nex
