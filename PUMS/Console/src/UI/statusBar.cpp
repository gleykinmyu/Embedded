#include "statusBar.hpp"

#include <cstdio>
#include <cstring>

#include "buttons.hpp"
#include "phl/rtc.hpp"

namespace server {

namespace {

constexpr uint16_t kFieldPad = 6u;
constexpr nex::FontId kFontId = 0u;
/** Высота глифа font0 в HMI (оценка ширины текста). */
constexpr uint16_t kFontHeightPx = 24u;

void copyFieldText(char* dst, size_t cap, const char* src) noexcept
{
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    std::strncpy(dst, src, cap - 1u);
    dst[cap - 1u] = '\0';
}

/** Обрезать @a src в @a dst так, чтобы оценка ширины ≤ @a maxPx (с `...`). */
void fitFieldText(char* dst, size_t cap, const char* src, uint16_t maxPx) noexcept
{
    if (dst == nullptr || cap == 0u) {
        return;
    }
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }

    copyFieldText(dst, cap, src);
    const nex::Font font{kFontId, kFontHeightPx};
    if (font.minWidthFor(dst, 0u) <= maxPx) {
        return;
    }

    constexpr const char kEllipsis[] = "...";
    constexpr std::size_t kEllipsisLen = sizeof(kEllipsis) - 1u;
    if (cap <= kEllipsisLen + 1u) {
        copyFieldText(dst, cap, kEllipsis);
        return;
    }

    /* Укорачиваем с конца, пока имя+`...` не влезет. */
    std::size_t len = std::strlen(dst);
    while (len > 0u) {
        --len;
        dst[len] = '\0';
        if (len + kEllipsisLen >= cap) {
            continue;
        }
        std::memcpy(dst + len, kEllipsis, kEllipsisLen + 1u);
        if (font.minWidthFor(dst, 0u) <= maxPx) {
            return;
        }
    }
    copyFieldText(dst, cap, kEllipsis);
}

} // namespace

StatusBar::StatusBar(const nex::Rect screen, const uint16_t barHeight) noexcept
    : _screen(screen)
    , _barHeight(barHeight)
{
    setRegion(nex::Region(nex::Point{0, kOriginY}, nex::Rect{screen.w, barHeight}));

    auto& status = column(Field::Status);
    status.width = kStatusColumnWidth;
    status.align = nex::HAlign::Left;

    auto& file = column(Field::File);
    file.fit = true;
    file.align = nex::HAlign::Center;

    auto& time = column(Field::Time);
    time.width = kSideColumnWidth;
    time.align = nex::HAlign::Right;

    addChildTop(status);
    addChildTop(file);
    addChildTop(time);
    layout();
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
    if (field == Field::File || field >= kFieldCount) {
        return;
    }
    column(field).setWidth(width);
}

void StatusBar::Column::setText(const char* const src) noexcept
{
    char buf[kTextCap]{};
    if (fit) {
        const uint16_t textW = (region().size.w > 2u * kFieldPad)
            ? static_cast<uint16_t>(region().size.w - 2u * kFieldPad)
            : 0u;
        fitFieldText(buf, kTextCap, src, textW);
    } else {
        copyFieldText(buf, kTextCap, src);
    }
    if (std::strcmp(text, buf) == 0) {
        return;
    }
    std::memcpy(text, buf, kTextCap);

    auto* const bar = static_cast<StatusBar*>(parent());
    if (bar != nullptr) {
        bar->present(*this);
    }
}

void StatusBar::Column::append(const char* const src) noexcept
{
    if (src == nullptr || src[0] == '\0') {
        return;
    }

    char buf[kTextCap]{};
    copyFieldText(buf, kTextCap, text);
    const std::size_t cur = std::strlen(buf);
    if (cur < kTextCap - 1u) {
        std::strncat(buf, src, kTextCap - 1u - cur);
        buf[kTextCap - 1u] = '\0';
    }
    setText(buf);
}

void StatusBar::Column::setWidth(const uint16_t w) noexcept
{
    if (width == w) {
        return;
    }
    width = w;
    auto* const bar = static_cast<StatusBar*>(parent());
    if (bar != nullptr) {
        bar->onColumnWidthChanged();
    }
}

void StatusBar::Column::draw(const nex::AppCanvas& cs) const
{
    if (text[0] == '\0') {
        return;
    }
    cs.text_in_region(screenRegion(), kFieldPad, text, kFontId, AppColors::kText, align,
        nex::VAlign::Center, AppColors::kPage, nex::BG::Color);
}

void StatusBar::setFile(const char* text, const bool edited) noexcept
{
    if (!edited) {
        column(Field::File).setText(text);
        return;
    }

    char withMark[kTextCap]{};
    copyFieldText(withMark, kTextCap, text);
    const std::size_t cur = std::strlen(withMark);
    if (cur < kTextCap - 1u) {
        withMark[cur] = '*';
        withMark[cur + 1u] = '\0';
    }
    column(Field::File).setText(withMark);
}

void StatusBar::setTime(const PHL::DateTime& dt) noexcept
{
    char buf[16]{};
    std::snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
        static_cast<unsigned>(dt.hour),
        static_cast<unsigned>(dt.minute),
        static_cast<unsigned>(dt.second));
    column(Field::Time).setText(buf);
}

void StatusBar::layout() noexcept
{
    const uint16_t totalW = _screen.w;
    const uint16_t h = _barHeight;
    const nex::Coord y = 0;

    auto& status = column(Field::Status);
    auto& file = column(Field::File);
    auto& time = column(Field::Time);

    const uint16_t statusW = (status.width > 0u) ? status.width : kStatusColumnWidth;
    const uint16_t timeW = (time.width > 0u) ? time.width : kSideColumnWidth;
    const uint16_t timeX =
        (totalW > timeW) ? static_cast<uint16_t>(totalW - timeW) : 0u;
    const uint16_t fileX = statusW;
    const uint16_t fileW = (totalW > statusW + timeW)
        ? static_cast<uint16_t>(totalW - statusW - timeW)
        : 0u;

    status.setRegion(nex::Region(nex::Point{0, y}, nex::Rect{statusW, h}));
    file.setRegion(nex::Region(nex::Point{static_cast<nex::Coord>(fileX), y}, nex::Rect{fileW, h}));
    time.setRegion(nex::Region(nex::Point{static_cast<nex::Coord>(timeX), y}, nex::Rect{timeW, h}));

    Widget::layout();
}

void StatusBar::drawBackground(const nex::AppCanvas& cs) const
{
    //cs.rect_fill(screenRegion(), AppColors::kPage);
}

void StatusBar::drawBackgroundRegion(const nex::AppCanvas& cs, const nex::Region clip) const
{
    //cs.rect_fill(clip, AppColors::kPage);
}

void StatusBar::onColumnWidthChanged() noexcept
{
    layout();
    presentAll();
}

void StatusBar::present(const nex::ovl::Object& obj) noexcept
{
    if (_overlay == nullptr || !isVisible() || _overlay->isModal()) {
        return;
    }
    redrawObject(obj, _overlay->app.cs);
}

void StatusBar::presentAll() noexcept
{
    if (_overlay == nullptr || !isVisible() || _overlay->isModal()) {
        return;
    }
    draw(_overlay->app.cs);
}

} // namespace server
