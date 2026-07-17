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

void StatusBar::layout() noexcept
{
    layoutColumns();
    Widget::layout();
}

void StatusBar::layoutColumns() noexcept
{
    const uint16_t totalW = _screen.w;
    uint16_t fixedW = 0u;
    uint8_t autoCount = 0u;

    for (const Column& col : _columns) {
        if (col.width > 0u) {
            fixedW = static_cast<uint16_t>(fixedW + col.width);
        } else {
            ++autoCount;
        }
    }

    uint16_t autoW = 0u;
    if (autoCount > 0u) {
        const uint16_t remain = (totalW > fixedW) ? static_cast<uint16_t>(totalW - fixedW) : 0u;
        autoW = static_cast<uint16_t>(remain / autoCount);
    }

    nex::Coord x = 0;
    const nex::Coord y = 0;
    const uint16_t h = _barHeight;

    for (Column& col : _columns) {
        const uint16_t w = (col.width > 0u) ? col.width : autoW;
        col.region = nex::Region(nex::Point{x, y}, nex::Rect{w, h});
        x = static_cast<nex::Coord>(x + w);
    }
}

void StatusBar::drawBackground(const nex::AppCanvas& cs) const
{
    const nex::Region bar = screenRegion();
    cs.rect_fill(bar, AppColors::kPage);

    for (const Column& col : _columns) {
        const nex::Region field = nex::Canvas::toScreen(bar, col.region);
        cs.text_in_region(field, kFieldPad, col.text, kFontId, AppColors::kText, col.align,
            nex::VAlign::Center, AppColors::kPage, nex::BG::Color);
    }
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
