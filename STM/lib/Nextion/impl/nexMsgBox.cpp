#include "nexMsgBox.hpp"
#include "nexApplication.hpp"
#include "../core/nexMessages.hpp"
#include "../core/nexProtocol.hpp"

#include <cstring>

namespace nex {

namespace {

constexpr uint16_t kBorder = 4u;
constexpr uint16_t kPad = 10u;
constexpr uint16_t kTitleH = 44u;
constexpr uint16_t kButtonH = 56u;
constexpr uint16_t kButtonGap = 10u;
constexpr uint16_t kButtonBottomLift = 8u;
constexpr uint16_t kButtonW = 140u;
constexpr FontId kFont = 1u;
/** Nextion `xstr`: xcen/ycen — 0 слева/сверху, 1 по центру, 2 справа/снизу. */
constexpr uint32_t kAlignCenter = 1u;
constexpr Literal kOkLabel{"OK"};
constexpr Literal kTitleError{"Error"};
constexpr Literal kTitleMessage{"Message"};
/** Текст `xstr` должен влезать в `TxFrame::MAX_PAYLOAD` вместе с координатами и числами. */
constexpr std::size_t kXstrWireOverhead = 80u;
constexpr std::size_t kMaxXstrText =
    (TxFrame::MAX_PAYLOAD > kXstrWireOverhead) ? static_cast<std::size_t>(TxFrame::MAX_PAYLOAD - kXstrWireOverhead) : 0u;

void clampTextForXstr(char* buf, std::size_t cap) noexcept {
    if (cap == 0u)
        return;
    const std::size_t maxLen = cap - 1u;
    const std::size_t limit = (maxLen < kMaxXstrText) ? maxLen : kMaxXstrText;
    if (std::strlen(buf) > limit)
        buf[limit] = '\0';
}

struct PanelRect {
    Point ul{};
    uint32_t w = 0u;
    uint32_t h = 0u;
};

[[nodiscard]] PanelRect mapUserRect(const ScreenLayout& screen, uint16_t ux0, uint16_t uy0, uint16_t ux1,
    uint16_t uy1) noexcept {
    Point lr{};
    PanelRect r{};
    screen.mapUserRectToPanel(ux0, uy0, ux1, uy1, r.ul, lr);
    if (lr.x >= r.ul.x && lr.y >= r.ul.y) {
        r.w = static_cast<uint32_t>(lr.x - r.ul.x + 1u);
        r.h = static_cast<uint32_t>(lr.y - r.ul.y + 1u);
    }
    return r;
}

} // namespace

MsgBox::MsgBox(Application& app) noexcept : _app(app) {}

bool MsgBox::contains(Point p, Point ul, Point lr) noexcept {
    return p.x >= ul.x && p.x <= lr.x && p.y >= ul.y && p.y <= lr.y;
}

bool MsgBox::okContains(Point p) const noexcept {
    return contains(p, _okUl, _okLr);
}

void MsgBox::drawOkButton(bool pressed) noexcept {
    const PanelRect ok = mapUserRect(_app.screenLayout(), _okUl.x, _okUl.y, _okLr.x, _okLr.y);
    if (ok.w == 0u || ok.h == 0u)
        return;

    Point lr{static_cast<uint16_t>(ok.ul.x + ok.w - 1u), static_cast<uint16_t>(ok.ul.y + ok.h - 1u)};
    const Color fill = pressed ? Color::std::Green : Color::std::Gray;
    const Color fg = Color::std::White;
    _app.cs.rect_fill(ok.ul, lr, fill);
    _app.cs.text_in_region(ok.ul, ok.w, ok.h, kFont, fg, fill, kAlignCenter, kAlignCenter, BGStyle::Color, kOkLabel.data);
}

void MsgBox::draw() noexcept {
    const ScreenLayout& screen = _app.screenLayout();

    Point panelUl{};
    Point panelLr{};
    screen.mapUserRectToPanel(_boxUl.x, _boxUl.y, _boxLr.x, _boxLr.y, panelUl, panelLr);

    const uint16_t fillUx0 = static_cast<uint16_t>(_boxUl.x + kBorder);
    const uint16_t fillUy0 = static_cast<uint16_t>(_boxUl.y + kBorder);
    const uint16_t fillUx1 = static_cast<uint16_t>(_boxLr.x - kBorder);
    const uint16_t fillUy1 = static_cast<uint16_t>(_boxLr.y - kBorder);
    Point fillUl{};
    Point fillLr{};
    screen.mapUserRectToPanel(fillUx0, fillUy0, fillUx1, fillUy1, fillUl, fillLr);

    _app.cs.rect_fill(panelUl, panelLr, Color::std::Red);
    _app.cs.rect_fill(fillUl, fillLr, Color::std::Navy);

    const uint16_t innerUx0 = static_cast<uint16_t>(_boxUl.x + kBorder);
    const uint16_t innerUx1 = static_cast<uint16_t>(_boxLr.x - kBorder);
    const uint16_t titleUy0 = static_cast<uint16_t>(_boxUl.y + kBorder);
    const uint16_t titleUy1 = static_cast<uint16_t>(titleUy0 + kTitleH - 1u);
    const PanelRect titleRect = mapUserRect(screen, innerUx0, titleUy0, innerUx1, titleUy1);
    const Literal& title = _isError ? kTitleError : kTitleMessage;
    const Color titleFg = _isError ? Color::std::Yellow : Color::std::White;
    if (titleRect.w > 0u && titleRect.h > 0u)
        _app.cs.text_in_region(titleRect.ul, titleRect.w, titleRect.h, kFont, titleFg, Color::std::Navy, kAlignCenter,
            kAlignCenter, BGStyle::Color, title.data);

    const uint16_t textUx0 = static_cast<uint16_t>(_boxUl.x + kPad);
    const uint16_t textUx1 = static_cast<uint16_t>(_boxLr.x - kPad);
    const uint16_t textUy0 = static_cast<uint16_t>(titleUy1 + kPad + 1u);
    const uint16_t textUy1 = static_cast<uint16_t>(_okUl.y - kButtonGap - 1u);
    const PanelRect bodyRect = mapUserRect(screen, textUx0, textUy0, textUx1, textUy1);

    if (bodyRect.w > 0u && bodyRect.h > 0u && _text[0] != '\0')
        _app.cs.text_in_region(bodyRect.ul, bodyRect.w, bodyRect.h, kFont, Color::std::White, Color::std::Navy,
            kAlignCenter, kAlignCenter, BGStyle::Color, _text);

    drawOkButton(_okPressed);
}

void MsgBox::setText(const char* text) noexcept {
    if (text == nullptr) {
        _text[0] = '\0';
        return;
    }
    std::strncpy(_text, text, kTextCap - 1u);
    _text[kTextCap - 1u] = '\0';
    clampTextForXstr(_text, kTextCap);
}

void MsgBox::show(const char* text) noexcept {
    _isError = false;
    setText(text);
    present();
}

void MsgBox::showError() noexcept {
    _isError = true;
    formatStatusMessage(_app.lastError(), _app.lastErrorPage(), _app.lastErrorComp(), _text, kTextCap);
    clampTextForXstr(_text, kTextCap);
    present();
}

void MsgBox::present() noexcept {
    const ScreenLayout& screen = _app.screenLayout();
    if (screen.width == 0u || screen.height == 0u)
        return;

    _app.touch.touchSwitch(false);

    const uint16_t uw = screen.userWidth();
    const uint16_t uh = screen.userHeight();
    if (uw < 32u || uh < 32u) {
        _app.touch.touchSwitch(true);
        return;
    }

    const uint16_t margin = static_cast<uint16_t>((uw < uh ? uw : uh) / 16u);
    uint16_t boxW = static_cast<uint16_t>((uw * 4u) / 5u);
    uint16_t boxH = static_cast<uint16_t>((uh * 2u) / 7u);
    if (boxW < 180u)
        boxW = 180u;
    if (boxH < 168u)
        boxH = 168u;
    const uint16_t maxW = static_cast<uint16_t>(uw - 2u * margin);
    const uint16_t maxH = static_cast<uint16_t>(uh - 2u * margin);
    if (boxW > maxW)
        boxW = maxW;
    if (boxH > maxH)
        boxH = maxH;

    _boxUl = Point{static_cast<uint16_t>((uw - boxW) / 2u), static_cast<uint16_t>((uh - boxH) / 2u)};
    _boxLr = Point{static_cast<uint16_t>(_boxUl.x + boxW - 1u), static_cast<uint16_t>(_boxUl.y + boxH - 1u)};

    uint16_t okW = kButtonW;
    if (okW > boxW - 2u * kPad)
        okW = static_cast<uint16_t>(boxW - 2u * kPad);
    if (okW < 80u)
        okW = 80u;

    const uint16_t okBottomY = static_cast<uint16_t>(_boxLr.y - kBorder - kButtonBottomLift);
    const uint16_t okTopY = static_cast<uint16_t>(okBottomY - kButtonH + 1u);
    const uint16_t okLeftX = static_cast<uint16_t>(_boxUl.x + (boxW - okW) / 2u);
    _okUl = Point{okLeftX, okTopY};
    _okLr = Point{static_cast<uint16_t>(okLeftX + okW - 1u), okBottomY};

    _okPressed = false;
    _active = true;
    draw();
    _app.touch.sendXY(true);
}

void MsgBox::onTouchXY(const msg::evTouchXY& e) noexcept {
    if (!_active)
        return;

    if (e.state == TouchState::Press) {
        if (okContains(e.pos)) {
            _okPressed = true;
            drawOkButton(true);
        }
        return;
    }

    if (e.state != TouchState::Release)
        return;

    if (!_okPressed)
        return;

    const bool accept = okContains(e.pos);
    _okPressed = false;
    if (accept) {
        dismiss();
        return;
    }
    drawOkButton(false);
}

void MsgBox::dismiss() noexcept {
    if (!_active)
        return;
    _active = false;
    _okPressed = false;
    _app.touch.sendXY(false);
    _app.touch.touchSwitch(true);
    _app.refreshPage();
}

} // namespace nex
