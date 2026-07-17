#include "statusBar.hpp"

#include <cstring>

#include "buttons.hpp"

namespace server {

namespace {

constexpr uint16_t kFieldPad = 6u;
constexpr nex::FontId kFontId = 0u;

void copyFieldText(char* dst, size_t cap, const char* src) noexcept
{
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    std::strncpy(dst, src, cap - 1u);
    dst[cap - 1u] = '\0';
}

} // namespace

StatusBar::StatusBar(const nex::Rect screen, const uint16_t barHeight) noexcept
    : _screen(screen)
    , _barHeight(barHeight)
{
    _columns[static_cast<uint8_t>(Field::Status)].width = kStatusColumnWidth;
    _columns[static_cast<uint8_t>(Field::Status)].align = nex::HAlign::Left;
    _columns[static_cast<uint8_t>(Field::File)].width = 0u;
    _columns[static_cast<uint8_t>(Field::File)].align = nex::HAlign::Center;
    _columns[static_cast<uint8_t>(Field::Time)].width = kSideColumnWidth;
    _columns[static_cast<uint8_t>(Field::Time)].align = nex::HAlign::Right;

    setRegion(nex::Region(nex::Point{0, kOriginY}, nex::Rect{screen.w, barHeight}));
    layoutColumns();
}

void StatusBar::show(nex::ovl::Overlay& ovl) noexcept
{
    _overlay = &ovl;
    Widget::show(ovl);
}

void StatusBar::hide(nex::ovl::Overlay& ovl) noexcept
{
    Widget::hide(ovl);
    if (_overlay == &ovl) {
        _overlay = nullptr;
    }
}

void StatusBar::setColumnWidth(const Field field, const uint16_t width) noexcept
{
    if (field == Field::File) {
        return;
    }
    const auto idx = static_cast<uint8_t>(field);
    if (idx >= static_cast<uint8_t>(Field::Count)) {
        return;
    }
    _columns[idx].width = width;
    layoutColumns();
    redrawIfShown();
}

void StatusBar::setField(const Field field, const char* text) noexcept
{
    const auto idx = static_cast<uint8_t>(field);
    if (idx >= static_cast<uint8_t>(Field::Count)) {
        return;
    }

    char buf[kTextCap]{};
    copyFieldText(buf, kTextCap, text);
    if (std::strcmp(_columns[idx].text, buf) == 0) {
        return;
    }
    std::memcpy(_columns[idx].text, buf, kTextCap);
    redrawIfShown();
}

void StatusBar::appendField(const Field field, const char* text) noexcept
{
    const auto idx = static_cast<uint8_t>(field);
    if (idx >= static_cast<uint8_t>(Field::Count) || text == nullptr || text[0] == '\0') {
        return;
    }

    const std::size_t cur = std::strlen(_columns[idx].text);
    if (cur >= kTextCap - 1u) {
        return;
    }
    std::strncat(_columns[idx].text, text, kTextCap - 1u - cur);
    _columns[idx].text[kTextCap - 1u] = '\0';
    redrawIfShown();
}

void StatusBar::setFile(const char* text, const bool edited) noexcept
{
    setFile(text);
    if (edited) {
        appendField(Field::File, "*");
    }
}

void StatusBar::layout() noexcept
{
    layoutColumns();
    Widget::layout();
}

void StatusBar::layoutColumns() noexcept
{
    const uint16_t totalW = _screen.w;
    const uint16_t h = _barHeight;
    const nex::Coord y = 0;

    auto& status = _columns[static_cast<uint8_t>(Field::Status)];
    auto& file = _columns[static_cast<uint8_t>(Field::File)];
    auto& time = _columns[static_cast<uint8_t>(Field::Time)];

    const uint16_t statusW = (status.width > 0u) ? status.width : kStatusColumnWidth;
    const uint16_t timeW = (time.width > 0u) ? time.width : kSideColumnWidth;
    const uint16_t timeX =
        (totalW > timeW) ? static_cast<uint16_t>(totalW - timeW) : 0u;

    /* File — на всю ширину бара: текст по центру экрана, не средней колонки. */
    status.region = nex::Region(nex::Point{0, y}, nex::Rect{statusW, h});
    file.region = nex::Region(nex::Point{0, y}, nex::Rect{totalW, h});
    time.region = nex::Region(nex::Point{static_cast<nex::Coord>(timeX), y}, nex::Rect{timeW, h});
}

void StatusBar::drawBackground(const nex::AppCanvas& cs) const
{
    const nex::Region bar = screenRegion();
    cs.rect_fill(bar, AppColors::kPage);

    const auto& status = _columns[static_cast<uint8_t>(Field::Status)];
    const auto& file = _columns[static_cast<uint8_t>(Field::File)];
    const auto& time = _columns[static_cast<uint8_t>(Field::Time)];

    const nex::Region fileField = nex::Canvas::toScreen(bar, file.region);
    cs.text_in_region(fileField, kFieldPad, file.text, kFontId, AppColors::kText, file.align,
        nex::VAlign::Center, AppColors::kPage, nex::BG::Transparent);

    const nex::Region statusField = nex::Canvas::toScreen(bar, status.region);
    cs.rect_fill(statusField, AppColors::kPage);
    cs.text_in_region(statusField, kFieldPad, status.text, kFontId, AppColors::kText, status.align,
        nex::VAlign::Center, AppColors::kPage, nex::BG::Color);

    const nex::Region timeField = nex::Canvas::toScreen(bar, time.region);
    cs.rect_fill(timeField, AppColors::kPage);
    cs.text_in_region(timeField, kFieldPad, time.text, kFontId, AppColors::kText, time.align,
        nex::VAlign::Center, AppColors::kPage, nex::BG::Color);
}

void StatusBar::redrawIfShown() const noexcept
{
    if (_overlay == nullptr || !isVisible()) {
        return;
    }
    // Не перебивать modal MsgBox; время догонит после закрытия.
    if (_overlay->isModal()) {
        return;
    }
    draw(_overlay->app.cs);
}

} // namespace server
