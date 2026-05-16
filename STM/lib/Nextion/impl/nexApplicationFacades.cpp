#include "nexApplicationFacades.hpp"
#include "nexApplication.hpp"

namespace nex {

// --- AppEeprom --------------------------------------------------------------

AppEeprom::AppEeprom(Application& a) noexcept : _app(a) {}

void AppEeprom::write(const cmd::TargetAttr& variableOrConstant, uint32_t eepromStart) const noexcept {
    _app.enqueue(Transaction{cmd::Eeprom::write(variableOrConstant, eepromStart), 0u, 0u});
}

void AppEeprom::read(const cmd::TargetAttr& destVariable, uint32_t eepromStart) const noexcept {
    _app.enqueue(Transaction{cmd::Eeprom::read(destVariable, eepromStart), 0u, 0u});
}

void AppEeprom::write_t(uint32_t eepromStart, const uint8_t* buffer, uint32_t byteCount) const noexcept {
    (void)buffer;
    _app.enqueue(
        Transaction{cmd::Eeprom::writeT(eepromStart, byteCount), 0u, 0u, 0u, Transaction::State::AwaitingTransparentTx});
}

void AppEeprom::read_t(uint32_t eepromStart, uint8_t* buffer, uint32_t byteCount) const noexcept {
    (void)buffer;
    _app.enqueue(Transaction{cmd::Eeprom::readT(eepromStart, byteCount), 0u, 0u, 0u, Transaction::State::AwaitingRawDataRx});
}

// --- AppFileSystem ----------------------------------------------------------

AppFileSystem::AppFileSystem(Application& a) noexcept : _app(a) {}

void AppFileSystem::file_remove(const char* pathQuoted) const noexcept {
    _app.enqueue(Transaction{cmd::File::remove(pathQuoted), 0u, 0u});
}

void AppFileSystem::file_rename(const char* pathFromQuoted, const char* pathToQuoted) const noexcept {
    _app.enqueue(Transaction{cmd::File::rename(pathFromQuoted, pathToQuoted), 0u, 0u});
}

void AppFileSystem::file_find(const char* pathQuoted, const cmd::TargetAttr& dstNumAttr) const noexcept {
    _app.enqueue(Transaction{cmd::File::find(pathQuoted, dstNumAttr), 0u, 0u});
}

void AppFileSystem::file_create(const char* pathQuoted, uint32_t reservedSize) const noexcept {
    _app.enqueue(Transaction{cmd::File::create(pathQuoted, reservedSize), 0u, 0u});
}

void AppFileSystem::file_read_t(const char* pathQuoted, uint32_t offset, uint8_t* buffer, uint32_t byteCount,
    uint32_t crcOption) const noexcept {
    (void)buffer;
    _app.enqueue(
        Transaction{cmd::File::read(pathQuoted, offset, byteCount, crcOption), 0u, 0u, 0u, Transaction::State::AwaitingRawDataRx});
}

void AppFileSystem::file_write_t(const char* pathQuoted, const uint8_t* buffer, uint32_t byteCount) const noexcept {
    (void)buffer;
    _app.enqueue(Transaction{cmd::File::writeT(pathQuoted, byteCount), 0u, 0u, 0u, Transaction::State::AwaitingTransparentTx});
}

void AppFileSystem::dir_remove(const char* pathQuoted) const noexcept {
    _app.enqueue(Transaction{cmd::Directory::remove(pathQuoted), 0u, 0u});
}

void AppFileSystem::dir_create(const char* pathQuoted) const noexcept {
    _app.enqueue(Transaction{cmd::Directory::create(pathQuoted), 0u, 0u});
}

void AppFileSystem::dir_rename(const char* pathFromQuoted, const char* pathToQuoted) const noexcept {
    _app.enqueue(Transaction{cmd::Directory::rename(pathFromQuoted, pathToQuoted), 0u, 0u});
}

void AppFileSystem::dir_find(const char* pathQuoted, const cmd::TargetAttr& dstNumAttr) const noexcept {
    _app.enqueue(Transaction{cmd::Directory::find(pathQuoted, dstNumAttr), 0u, 0u});
}

// --- AppCanvas --------------------------------------------------------------

AppCanvas::AppCanvas(Application& a) noexcept : _app(a) {}

void AppCanvas::clear_screen(Color color) const noexcept {
    _app.enqueue(Transaction{cmd::gui::ClearScreen(color), 0u, 0u});
}

void AppCanvas::picture(Point at, PicId pictureId) const noexcept {
    _app.enqueue(Transaction{cmd::gui::Picture(at, pictureId), 0u, 0u});
}

void AppCanvas::picture_in_place(Point upperLeft, uint32_t w, uint32_t h, PicId pictureId) const noexcept {
    _app.enqueue(Transaction{cmd::gui::PictureCrop::inPlace(upperLeft, w, h, pictureId), 0u, 0u});
}

void AppCanvas::picture_draw(Point dst, uint32_t w, uint32_t h, Point src, PicId pictureId) const noexcept {
    _app.enqueue(Transaction{cmd::gui::PictureCrop::draw(dst, w, h, src, pictureId), 0u, 0u});
}

void AppCanvas::text_in_region(Point upperLeft, uint32_t w, uint32_t h, FontId fontId, Color fg, Color bg,
    uint32_t hAlign, uint32_t vAlign, BGStyle fill, const char* contentToken) const noexcept {
    _app.enqueue(Transaction{
        cmd::gui::TextInRegion(upperLeft, w, h, fontId, fg, bg, hAlign, vAlign, fill, contentToken), 0u, 0u});
}

void AppCanvas::rect_fill(Point upperLeft, Point lowerRightInclusive, Color color) const noexcept {
    _app.enqueue(
        Transaction{cmd::gui::Rect(cmd::gui::Rect::Mode::Fill, upperLeft, lowerRightInclusive, color), 0u, 0u});
}

void AppCanvas::rect_outline(Point upperLeft, Point lowerRightInclusive, Color color) const noexcept {
    _app.enqueue(
        Transaction{cmd::gui::Rect(cmd::gui::Rect::Mode::Outline, upperLeft, lowerRightInclusive, color), 0u, 0u});
}

void AppCanvas::line(Point from, Point to, Color color) const noexcept {
    _app.enqueue(Transaction{cmd::gui::Line(from, to, color), 0u, 0u});
}

void AppCanvas::circle_outline(Point center, uint32_t radius, Color color) const noexcept {
    _app.enqueue(Transaction{cmd::gui::Circle(cmd::gui::Circle::Kind::Outline, center, radius, color), 0u, 0u});
}

void AppCanvas::circle_filled(Point center, uint32_t radius, Color color) const noexcept {
    _app.enqueue(Transaction{cmd::gui::Circle(cmd::gui::Circle::Kind::Filled, center, radius, color), 0u, 0u});
}

} // namespace nex
