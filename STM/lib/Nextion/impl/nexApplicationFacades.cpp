#include "nexApplicationFacades.hpp"
#include "nexApplication.hpp"

namespace nex {

// --- AppEeprom --------------------------------------------------------------

AppEeprom::AppEeprom(Application& a) noexcept : _app(a) {}

bool AppEeprom::write(const cmd::TargetAttr& variableOrConstant, uint32_t eepromStart) const noexcept {
    return _app.pushCommand(cmd::Eeprom::write(variableOrConstant, eepromStart));
}

bool AppEeprom::read(const cmd::TargetAttr& destVariable, uint32_t eepromStart) const noexcept {
    return _app.pushCommand(cmd::Eeprom::read(destVariable, eepromStart));
}

bool AppEeprom::write_t(uint32_t eepromStart, const uint8_t* buffer, uint32_t byteCount) const noexcept {
    return _app.requestTransparentTransmit(cmd::Eeprom::writeT(eepromStart, byteCount), buffer, byteCount);
}

bool AppEeprom::read_t(uint32_t eepromStart, uint8_t* buffer, uint32_t byteCount) const noexcept {
    return _app.requestTransparentReceive(cmd::Eeprom::readT(eepromStart, byteCount), buffer, byteCount);
}

// --- AppFileSystem ----------------------------------------------------------

AppFileSystem::AppFileSystem(Application& a) noexcept : _app(a) {}

bool AppFileSystem::file_remove(const char* pathQuoted) const noexcept {
    return _app.pushCommand(cmd::File::remove(pathQuoted));
}

bool AppFileSystem::file_rename(const char* pathFromQuoted, const char* pathToQuoted) const noexcept {
    return _app.pushCommand(cmd::File::rename(pathFromQuoted, pathToQuoted));
}

bool AppFileSystem::file_find(const char* pathQuoted, const cmd::TargetAttr& dstNumAttr) const noexcept {
    return _app.pushCommand(cmd::File::find(pathQuoted, dstNumAttr));
}

bool AppFileSystem::file_create(const char* pathQuoted, uint32_t reservedSize) const noexcept {
    return _app.pushCommand(cmd::File::create(pathQuoted, reservedSize));
}

bool AppFileSystem::file_read_t(const char* pathQuoted, uint32_t offset, uint8_t* buffer, uint32_t byteCount, uint32_t crcOption) const noexcept {
    return _app.requestTransparentReceive(cmd::File::read(pathQuoted, offset, byteCount, crcOption), buffer, byteCount);
}

bool AppFileSystem::file_write_t(const char* pathQuoted, const uint8_t* buffer, uint32_t byteCount) const noexcept {
    return _app.requestTransparentTransmit(cmd::File::writeT(pathQuoted, byteCount), buffer, byteCount);
}

bool AppFileSystem::dir_remove(const char* pathQuoted) const noexcept {
    return _app.pushCommand(cmd::Directory::remove(pathQuoted));
}

bool AppFileSystem::dir_create(const char* pathQuoted) const noexcept {
    return _app.pushCommand(cmd::Directory::create(pathQuoted));
}

bool AppFileSystem::dir_rename(const char* pathFromQuoted, const char* pathToQuoted) const noexcept {
    return _app.pushCommand(cmd::Directory::rename(pathFromQuoted, pathToQuoted));
}

bool AppFileSystem::dir_find(const char* pathQuoted, const cmd::TargetAttr& dstNumAttr) const noexcept {
    return _app.pushCommand(cmd::Directory::find(pathQuoted, dstNumAttr));
}

// --- AppCanvas --------------------------------------------------------------

AppCanvas::AppCanvas(Application& a) noexcept : _app(a) {}

bool AppCanvas::clear_screen(Color color) const noexcept {
    return _app.pushCommand(cmd::gui::ClearScreen(color));
}

bool AppCanvas::picture(Point at, PicId pictureId) const noexcept {
    return _app.pushCommand(cmd::gui::Picture(at, pictureId));
}

bool AppCanvas::picture_in_place(Point upperLeft, uint32_t w, uint32_t h, PicId pictureId) const noexcept {
    return _app.pushCommand(cmd::gui::PictureCrop::inPlace(upperLeft, w, h, pictureId));
}

bool AppCanvas::picture_draw(Point dst, uint32_t w, uint32_t h, Point src, PicId pictureId) const noexcept {
    return _app.pushCommand(cmd::gui::PictureCrop::draw(dst, w, h, src, pictureId));
}

bool AppCanvas::text_in_region(Point upperLeft, uint32_t w, uint32_t h, FontId fontId, Color fg, Color bg,
    uint32_t hAlign, uint32_t vAlign, BGStyle fill, const char* contentToken) const noexcept {
    return _app.pushCommand(
        cmd::gui::TextInRegion(upperLeft, w, h, fontId, fg, bg, hAlign, vAlign, fill, contentToken));
}

bool AppCanvas::rect_fill(Point upperLeft, Point lowerRightInclusive, Color color) const noexcept {
    return _app.pushCommand(
        cmd::gui::Rect(cmd::gui::Rect::Mode::Fill, upperLeft, lowerRightInclusive, color));
}

bool AppCanvas::rect_outline(Point upperLeft, Point lowerRightInclusive, Color color) const noexcept {
    return _app.pushCommand(
        cmd::gui::Rect(cmd::gui::Rect::Mode::Outline, upperLeft, lowerRightInclusive, color));
}

bool AppCanvas::line(Point from, Point to, Color color) const noexcept {
    return _app.pushCommand(cmd::gui::Line(from, to, color));
}

bool AppCanvas::circle_outline(Point center, uint32_t radius, Color color) const noexcept {
    return _app.pushCommand(cmd::gui::Circle(cmd::gui::Circle::Kind::Outline, center, radius, color));
}

bool AppCanvas::circle_filled(Point center, uint32_t radius, Color color) const noexcept {
    return _app.pushCommand(cmd::gui::Circle(cmd::gui::Circle::Kind::Filled, center, radius, color));
}

} // namespace nex
