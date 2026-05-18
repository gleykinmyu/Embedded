#include "nexMsgBox.hpp"
#include "../app/nexApplication.hpp"
#include "../core/nexProtocol.hpp"

#include <cstdio>
#include <cstring>

namespace nex {

namespace {

constexpr uint16_t kBorder = 4u;
constexpr uint16_t kPad = 10u;
constexpr uint16_t kTitleH = 44u;
constexpr uint16_t kButtonH = 56u;
constexpr uint16_t kButtonGap = 10u;
constexpr uint16_t kButtonRowGap = 8u;
constexpr uint16_t kButtonBottomLift = 8u;
constexpr uint16_t kButtonW = 140u;
constexpr uint16_t kButtonWMulti = 96u;
constexpr uint16_t kButtonWCancel = 140u;
constexpr Font kTitleFont{1u, 24u};
constexpr Font kBtnFont{1u, 24u};
constexpr Color kFrameBg{Color(0x4208u)};
constexpr Color kFrameBorder{Color(0x7BEFu)};
constexpr Color kBtnBg{Color(0x528Au)};
constexpr Color kBtnPressed{Color::std::Green};
constexpr Color kBtnBorder{Color(0x7BEFu)};
constexpr Color kBtnDefaultBorder{Color::std::White};
constexpr uint16_t kBtnBorderTh = 3u;
constexpr uint16_t kBtnDefaultBorderTh = 2u;

constexpr Literal kOkLabel{"OK"};
constexpr Literal kYesLabel{"Yes"};
constexpr Literal kNoLabel{"No"};
constexpr Literal kCancelLabel{"Cancel"};
constexpr Literal kTitleError{"Error"};
constexpr Literal kTitleMessage{"Message"};

const char* touchStateTag(const TouchState state) noexcept {
    return state == TouchState::Press ? "P" : "R";
}

void clampTextForXstr(char* buf, const std::size_t cap) noexcept {
    if (cap == 0u)
        return;
    const std::size_t maxLen = cap - 1u;
    const std::size_t limit = (maxLen < maxXstrTextLength()) ? maxLen : maxXstrTextLength();
    if (std::strlen(buf) > limit)
        buf[limit] = '\0';
}

/** Y верхнего края кнопки в ряду (все кнопки на одной линии). `boxBottomY` — нижний Y рамки окна. */
[[nodiscard]] uint16_t buttonTopY(const uint16_t boxBottomY) noexcept {
    const uint16_t bottomY = static_cast<uint16_t>(boxBottomY - kBorder - kButtonBottomLift);
    return static_cast<uint16_t>(bottomY - kButtonH + 1u);
}

} // namespace

MsgBox::Action MsgBox::defaultForPreset(const Preset preset) noexcept {
    switch (preset) {
    case Preset::OK:
        return Action::Ok;
    case Preset::OKCancel:
    case Preset::YesNoCancel:
        return Action::Cancel;
    case Preset::YesNo:
        return Action::None;
    default:
        return Action::Ok;
    }
}

const char* MsgBox::actionCstr(const Action action) noexcept {
    switch (action) {
    case Action::None: return "-";
    case Action::Ok: return "OK";
    case Action::Yes: return "Yes";
    case Action::No: return "No";
    case Action::Cancel: return "Cancel";
    default: return "?";
    }
}

const char* MsgBox::presetCstr(const Preset preset) noexcept {
    switch (preset) {
    case Preset::OK: return "OK";
    case Preset::OKCancel: return "OKCancel";
    case Preset::YesNo: return "YesNo";
    case Preset::YesNoCancel: return "YesNoCancel";
    default: return "?";
    }
}

MsgBox::Box::Box() noexcept
    : cancelBtn(kCancelLabel.data, kBtnFont, Rect(kButtonWCancel, kButtonH), kBtnBg, kBtnPressed, kBtnBorder,
          kBtnBorderTh),
      noBtn(kNoLabel.data, kBtnFont, Rect(kButtonWMulti, kButtonH), kBtnBg, kBtnPressed, kBtnBorder, kBtnBorderTh),
      okBtn(kOkLabel.data, kBtnFont, Rect(kButtonW, kButtonH), kBtnBg, kBtnPressed, kBtnBorder, kBtnBorderTh),
      yesBtn(kYesLabel.data, kBtnFont, Rect(kButtonWMulti, kButtonH), kBtnBg, kBtnPressed, kBtnBorder, kBtnBorderTh) {}

MsgBox::MsgBox(Application& app) noexcept : _app(app) {}

void MsgBox::Box::fit(const Rect& screen, const Preset preset) noexcept {
    const unsigned margin = (screen.w < screen.h ? screen.w : screen.h) / 16;

    // Размер рамки MsgBox — доля экрана, затем clamp по min и по экрану с отступом margin.
    frame.size = Rect(screen, 4, 5, 2, 7);
    if (frame.size.w < 180u)
        frame.size.w = 180u;
    if (frame.size.h < 168u)
        frame.size.h = 168u;
    if (preset == Preset::YesNoCancel && frame.size.w < 240u)
        frame.size.w = 240u;

    const Rect maxSize(static_cast<uint16_t>(screen.w - 2u * margin),
        static_cast<uint16_t>(screen.h - 2u * margin));
    if (frame.size.w > maxSize.w)
        frame.size.w = maxSize.w;
    if (frame.size.h > maxSize.h)
        frame.size.h = maxSize.h;

    // Верхний левый угол окна: центрирование frame на экране.
    frame.ul = Canvas::center(screen, frame.size);
}

void MsgBox::Box::hideButtons() noexcept {
    cancelBtn.hide();
    noBtn.hide();
    okBtn.hide();
    yesBtn.hide();
}

bool MsgBox::Box::hitAction(const Point p, Action& out) const noexcept {
    if (cancelBtn.contains(p)) {
        out = Action::Cancel;
        return true;
    }
    if (noBtn.contains(p)) {
        out = Action::No;
        return true;
    }
    if (okBtn.contains(p)) {
        out = Action::Ok;
        return true;
    }
    if (yesBtn.contains(p)) {
        out = Action::Yes;
        return true;
    }
    return false;
}

bool MsgBox::Box::hitPressedButton(const Point p, const Action pressed) const noexcept {
    switch (pressed) {
    case Action::Cancel:
        return cancelBtn.contains(p);
    case Action::No:
        return noBtn.contains(p);
    case Action::Ok:
        return okBtn.contains(p);
    case Action::Yes:
        return yesBtn.contains(p);
    default:
        return false;
    }
}

void MsgBox::Box::applyButtonColors(const Action defaultAction) noexcept {
    const auto style = [defaultAction](Canvas::Button& btn, const Action action) noexcept {
        const bool isDefault = defaultAction != Action::None && defaultAction == action;
        btn.setStyle(isDefault ? kBtnBorder : kBtnBg, kBtnPressed, isDefault ? kBtnDefaultBorder : kBtnBorder,
            isDefault ? kBtnDefaultBorderTh : kBtnBorderTh);
    };
    style(cancelBtn, Action::Cancel);
    style(noBtn, Action::No);
    style(okBtn, Action::Ok);
    style(yesBtn, Action::Yes);
}

void MsgBox::Box::drawButton(const AppCanvas& cs, const Action action, const bool pressing, const Action pressedAction) const noexcept {
    const bool fingerDown = pressing && pressedAction == action;
    switch (action) {
    case Action::Cancel: cancelBtn.draw(cs, fingerDown); break;
    case Action::No: noBtn.draw(cs, fingerDown); break;
    case Action::Ok: okBtn.draw(cs, fingerDown); break;
    case Action::Yes: yesBtn.draw(cs, fingerDown); break;
    default: break;
    }
}

void MsgBox::Box::draw(const AppCanvas& cs, const char* title, const char* text, const Color titleFg,
    const Action pressedAction, const bool pressing) const noexcept {
    cs.rect_bordered(frame, kFrameBg, kFrameBorder, kBorder);

    // Полоса заголовка: от frame.ul, высота kTitleH + рамка; текст с отступом kBorder от левого края.
    if (title != nullptr && title[0] != '\0')
        cs.text_in_region(Region(frame.ul, Rect(frame.size.w, static_cast<uint16_t>(kTitleH + 2u * kBorder))),
            kBorder, title, kTitleFont.fontId(), titleFg);

    // Нижний Y тела сообщения (включительно): сразу над рядом кнопок.
    const uint16_t textUy1 = static_cast<uint16_t>(buttonTopY(frame.lowerRight().y) - kButtonGap - 1u);

    // Верхний Y тела: под заголовком и боковым паддингом.
    const uint16_t textUy0 = static_cast<uint16_t>(frame.ul.y + kBorder + kTitleH + kPad + 1u);
    if (textUy0 <= textUy1 && text != nullptr && text[0] != '\0') {
        // Прямоугольник тела: bodyUl — верхний левый, bodySize — между боковыми pad и [textUy0..textUy1].
        const Point bodyUl(static_cast<uint16_t>(frame.ul.x + kPad), textUy0);
        const Rect bodySize(static_cast<uint16_t>(frame.size.w - 2u * kPad),
            static_cast<uint16_t>(textUy1 - textUy0 + 1u));
        if (bodySize.w > 0u && bodySize.h > 0u)
            cs.text_in_region(Region(bodyUl, bodySize), text, kTitleFont.fontId());
    }

    drawButtons(cs, pressing, pressedAction);
}

void MsgBox::Box::drawButtons(const AppCanvas& cs, const bool pressing, const Action pressedAction) const noexcept {
    drawButton(cs, Action::Cancel, pressing, pressedAction);
    drawButton(cs, Action::No, pressing, pressedAction);
    drawButton(cs, Action::Ok, pressing, pressedAction);
    drawButton(cs, Action::Yes, pressing, pressedAction);
}

void MsgBox::Box::layoutButtons(const Preset preset, const Action defaultAction) noexcept {
    hideButtons();

    // Общий Y верхнего левого угла всех кнопок ряда.
    const uint16_t topY = buttonTopY(frame.lowerRight().y);
    // Несколько кнопок: rowW — ширина ряда, x — левый X ряда (центр frame); соседи — x + w + kButtonRowGap.

    switch (preset) {
    case Preset::OK: {
        // X: одна кнопка по центру ширины окна.
        const uint16_t x = static_cast<uint16_t>(frame.ul.x + Canvas::center(frame.size, okBtn.size()).x);
        okBtn.show(Point(x, topY));
        break;
    }
    case Preset::OKCancel: {
        const uint16_t rowW = static_cast<uint16_t>(okBtn.w() + kButtonRowGap + cancelBtn.w());
        const uint16_t x = static_cast<uint16_t>(frame.ul.x + Canvas::center(frame.size, Rect(rowW, 0)).x);
        okBtn.show(Point(x, topY));
        cancelBtn.show(Point(static_cast<uint16_t>(x + okBtn.w() + kButtonRowGap), topY));
        break;
    }
    case Preset::YesNo: {
        const uint16_t rowW = static_cast<uint16_t>(yesBtn.w() + kButtonRowGap + noBtn.w());
        const uint16_t x = static_cast<uint16_t>(frame.ul.x + Canvas::center(frame.size, Rect(rowW, 0)).x);
        yesBtn.show(Point(x, topY));
        noBtn.show(Point(static_cast<uint16_t>(x + yesBtn.w() + kButtonRowGap), topY));
        break;
    }
    case Preset::YesNoCancel: {
        const uint16_t rowW =
            static_cast<uint16_t>(yesBtn.w() + 2u * kButtonRowGap + noBtn.w() + cancelBtn.w());
        const uint16_t x = static_cast<uint16_t>(frame.ul.x + Canvas::center(frame.size, Rect(rowW, 0)).x);
        yesBtn.show(Point(x, topY));
        noBtn.show(Point(static_cast<uint16_t>(x + yesBtn.w() + kButtonRowGap), topY));
        cancelBtn.show(
            Point(static_cast<uint16_t>(x + yesBtn.w() + kButtonRowGap + noBtn.w() + kButtonRowGap), topY));
        break;
    }
    }

    applyButtonColors(defaultAction);
}

void MsgBox::setTitle(const char* title) noexcept {
    if (title == nullptr) {
        _title[0] = '\0';
        return;
    }
    std::strncpy(_title, title, kTitleCap - 1u);
    _title[kTitleCap - 1u] = '\0';
    clampTextForXstr(_title, kTitleCap);
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

void MsgBox::setTextV(const char* fmt, va_list ap) noexcept {
    if (fmt == nullptr) {
        _text[0] = '\0';
        return;
    }
    std::vsnprintf(_text, kTextCap, fmt, ap);
    _text[kTextCap - 1u] = '\0';
    clampTextForXstr(_text, kTextCap);
}

void MsgBox::showFormatted(const char* title, const Preset preset, const Action defaultAction, const char* fmt,
    va_list ap) noexcept {
    _isError = false;
    _preset = preset;
    _defaultAction = defaultAction;
    setTitle(title != nullptr ? title : kTitleMessage.data);
    setTextV(fmt, ap);
    present();
}

void MsgBox::show(const Preset preset, const char* fmt, ...) noexcept {
    va_list ap;
    va_start(ap, fmt);
    showFormatted(kTitleMessage.data, preset, defaultForPreset(preset), fmt, ap);
    va_end(ap);
}

void MsgBox::show(const Preset preset, const Action defaultAction, const char* fmt, ...) noexcept {
    va_list ap;
    va_start(ap, fmt);
    showFormatted(kTitleMessage.data, preset, defaultAction, fmt, ap);
    va_end(ap);
}

void MsgBox::show(const char* title, const Preset preset, const char* fmt, ...) noexcept {
    va_list ap;
    va_start(ap, fmt);
    showFormatted(title, preset, defaultForPreset(preset), fmt, ap);
    va_end(ap);
}

void MsgBox::show(const char* title, const Preset preset, const Action defaultAction, const char* fmt, ...) noexcept {
    va_list ap;
    va_start(ap, fmt);
    showFormatted(title, preset, defaultAction, fmt, ap);
    va_end(ap);
}

void MsgBox::show(const char* text, const Preset preset) noexcept {
    show(text, preset, defaultForPreset(preset));
}

void MsgBox::show(const char* text, const Preset preset, const Action defaultAction) noexcept {
    _isError = false;
    _preset = preset;
    _defaultAction = defaultAction;
    setTitle(kTitleMessage.data);
    setText(text);
    present();
}

void MsgBox::show(const char* title, const char* text, const Preset preset) noexcept {
    show(title, text, preset, defaultForPreset(preset));
}

void MsgBox::show(const char* title, const char* text, const Preset preset, const Action defaultAction) noexcept {
    _isError = false;
    _preset = preset;
    _defaultAction = defaultAction;
    setTitle(title != nullptr ? title : kTitleMessage.data);
    setText(text);
    present();
}

void MsgBox::showError(const Preset preset) noexcept {
    showError(preset, defaultForPreset(preset));
}

void MsgBox::showError(const Preset preset, const Action defaultAction) noexcept {
    _isError = true;
    _preset = preset;
    _defaultAction = defaultAction;
    setTitle(kTitleError.data);
    formatStatusMessage(_app.lastError(), _app.lastErrorPage(), _app.lastErrorComp(), _text, kTextCap);
    clampTextForXstr(_text, kTextCap);
    present();
}

void MsgBox::notify(const Action action) noexcept {
    Event e{};
    e.action = action;
    e.isError = _isError;
    _app.onMsgBox(e);
}

void MsgBox::present() noexcept {
    const ScreenLayout& screen = _app.screenLayout();
    if (screen.size.w == 0u || screen.size.h == 0u || screen.size.w < 32u || screen.size.h < 32u)
        return;

    _app.touch.touchSwitch(false);

    _box.fit(screen.size, _preset);
    _box.layoutButtons(_preset, _defaultAction);

    _pressing = false;
    _pressedAction = Action::None;
    _active = true;
    const Color titleFg = _isError ? Color::std::Yellow : Color::std::White;
    _box.draw(_app.cs, _title, _text, titleFg, _pressedAction, false);
    _app.touch.sendXY(true);
}

void MsgBox::onButtonClick(const Action action) noexcept {
    notify(action);
    dismiss();
}

void MsgBox::onTouchXY(const msg::evTouchXY& e) noexcept {
    if (!_active)
        return;

    // Debug: print touch position and state
    //std::printf("[MB] %u,%u %s\n", static_cast<unsigned>(e.pos.x), static_cast<unsigned>(e.pos.y), touchStateTag(e.state));

    if (e.state == TouchState::Press) {
        if (_pressing)
            return;
        Action hit;
        if (_box.hitAction(e.pos, hit)) {
            _pressing = true;
            _pressedAction = hit;
            _box.drawButton(_app.cs, hit, true, hit);
        }
        return;
    }
    if (e.state != TouchState::Release || !_pressing)
        return;

    const Action pressed = _pressedAction;
    _pressing = false;

    if (pressed != Action::None && _box.hitPressedButton(e.pos, pressed)) {
        onButtonClick(pressed);
        return;
    }
    _box.drawButtons(_app.cs, false, pressed);
}

void MsgBox::dismiss() noexcept {
    if (!_active)
        return;
    _active = false;
    _pressing = false;
    _pressedAction = Action::None;
    _app.touch.sendXY(false);
    _app.touch.touchSwitch(true);
    _app.refreshPage();
}

} // namespace nex
