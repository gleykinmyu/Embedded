#include "ovlMsgBox.hpp"

#include "../../app/nexOvlApp.hpp"
#include "../../app/nexErrors.hpp"
#include "../../comp/nexCanvas.hpp"

#include <cstdio>
#include <cstring>

namespace nex::ovl {

namespace {

void clampTextForXstr(char* buf, const uint16_t capacity) noexcept {
    if (capacity == 0u)
        return;
    const uint16_t limit = min((uint16_t)(capacity - 1u), cmd::gui::TextInRegion::maxTextLength);
    if (std::strlen(buf) > limit)
        buf[limit] = '\0';
}

[[nodiscard]] Coord buttonTopY(const Coord boxBottomY) noexcept {
    const Coord bottomY = static_cast<Coord>(boxBottomY - kMsgBoxFrame.border.thickness - kMsgBoxBtn.bottomLift);
    return static_cast<Coord>(bottomY - kMsgBoxBtn.h + 1u);
}

[[nodiscard]] Region bodyTextRegion(const Region& frame) noexcept {
    const uint16_t border = kMsgBoxFrame.border.thickness;
    const Coord uy1 = static_cast<Coord>(buttonTopY(frame.lowerRight().y) - kMsgBoxBtn.gap - 1u);
    const Coord uy0 = static_cast<Coord>(frame.ul.y + border + kMsgBoxFrame.titleH + kMsgBoxFrame.pad + 1u);
    if (uy0 > uy1)
        return Region{};
    return Region(
        Point(static_cast<Coord>(frame.ul.x + kMsgBoxFrame.pad), uy0),
        Rect(static_cast<uint16_t>(frame.size.w - 2u * kMsgBoxFrame.pad), static_cast<uint16_t>(uy1 - uy0 + 1u)));
}

void layoutButtonRow(const Region& frame, const Coord topY, const uint16_t gap, Button* const* btns,
    const uint8_t count) noexcept {
    if (count == 0u)
        return;

    uint16_t rowW = btns[0]->region().size.w;
    for (uint8_t i = 1u; i < count; ++i)
        rowW = static_cast<uint16_t>(rowW + gap + btns[i]->region().size.w);

    Coord x = static_cast<Coord>(frame.ul.x + Canvas::center(frame.size, Rect(rowW, 0)).x);
    for (uint8_t i = 0u; i < count; ++i) {
        Button& btn = *btns[i];
        btn.setRegion(Region(
            Point(static_cast<Coord>(x - frame.ul.x), static_cast<Coord>(topY - frame.ul.y)), btn.region().size));
        btn.setVisible(true);
        if (i + 1u < count)
            x = static_cast<Coord>(x + btn.region().size.w + gap);
    }
}

} // namespace

constexpr uint8_t kPresetCount[] = {1u, 2u, 2u, 3u};

/** Индексы в `_btns`: 0 OK, 1 Cancel, 2 Yes, 3 No. */
constexpr uint8_t kPresetBtns[][3] = {
    {0u, 0u, 0u},
    {0u, 1u, 0u},
    {2u, 3u, 0u},
    {2u, 3u, 1u},
};

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

MsgBox::MsgBox(OvlApp& app) noexcept
    : _app(app),
      _screenSize(app.screenLayout().size),
      _btns{
          MsgButton{kMsgBoxLabels.ok, Rect(kMsgBoxBtn.w, kMsgBoxBtn.h), Action::Ok, kMsgBoxBtnStyles.normal},
          MsgButton{kMsgBoxLabels.cancel, Rect(kMsgBoxBtn.wCancel, kMsgBoxBtn.h), Action::Cancel, kMsgBoxBtnStyles.normal},
          MsgButton{kMsgBoxLabels.yes, Rect(kMsgBoxBtn.wMulti, kMsgBoxBtn.h), Action::Yes, kMsgBoxBtnStyles.normal},
          MsgButton{kMsgBoxLabels.no, Rect(kMsgBoxBtn.wMulti, kMsgBoxBtn.h), Action::No, kMsgBoxBtnStyles.normal},
      } {
    static constexpr uint8_t kChildOrder[] = {1u, 3u, 0u, 2u};
    for (const uint8_t idx : kChildOrder)
        addChildTop(_btns[idx]);
}

void MsgBox::setRoute(const Route route) noexcept {
    _ev.route = route;
    _routePinned = true;
}

void MsgBox::applyShow(const Preset preset, const Action defaultAction, const uint8_t tag) noexcept {
    _ev.tag = tag;
    _preset = preset;
    _defaultAction = defaultAction != Action::None ? defaultAction : defaultForPreset(preset);
}

void MsgBox::fit(const Rect& screen) noexcept {
    const unsigned margin = min(screen.w, screen.h) / 16;

    Region frame;
    frame.size = Rect(screen, 4, 5, 2, 7);

    const Rect maxSize(static_cast<uint16_t>(screen.w - 2u * margin),
        static_cast<uint16_t>(screen.h - 2u * margin));
    frame.size.w = clamp(frame.size.w, uint16_t{240}, maxSize.w);
    frame.size.h = clamp(frame.size.h, uint16_t{168}, maxSize.h);

    frame.ul = Canvas::center(screen, frame.size);
    setRegion(frame);
}

void MsgBox::layout() noexcept {
    if (!hasScreen())
        return;

    fit(_screenSize);

    const uint8_t presetIdx = static_cast<uint8_t>(_preset);
    const uint8_t count = kPresetCount[presetIdx];

    for (MsgButton& btn : _btns)
        btn.setVisible(false);

    const Region& frame = region();
    const Coord topY = buttonTopY(frame.lowerRight().y);
    Button* row[3]{};

    for (uint8_t i = 0u; i < count; ++i) {
        MsgButton& btn = _btns[kPresetBtns[presetIdx][i]];
        const bool isDefault = _defaultAction != Action::None && _defaultAction == btn.action();
        btn.setHighlighted(kMsgBoxBtnStyles, isDefault);
        row[i] = &btn;
    }

    layoutButtonRow(frame, topY, kMsgBoxBtn.rowGap, row, count);
}

void MsgBox::drawBackground(const AppCanvas& cs) const {
    const Region frame = screenRegion();
    const uint16_t border = kMsgBoxFrame.border.thickness;

    cs.rect_bordered(frame, kMsgBoxFrame.bg, kMsgBoxFrame.border.color, border);

    if (_title[0] != '\0') {
        const Color titleFg = (_ev.tag == msg::evMsgBox::kTagError) ? Color::std::Yellow : kMsgBoxFrame.font.color;
        cs.text_in_region(Region(frame.ul, Rect(frame.size.w, static_cast<uint16_t>(kMsgBoxFrame.titleH + 2u * border))),
            border, _title, kMsgBoxFrame.font.id, titleFg);
    }

    if (_text[0] != '\0') {
        const Region body = bodyTextRegion(frame);
        if (body.size.w > 0u && body.size.h > 0u)
            cs.text_in_region(body, _text, kMsgBoxFrame.font.id);
    }
}

void MsgBox::drawBackgroundRegion(const AppCanvas& cs, const Region clip) const {
    cs.rect_fill(clip, kMsgBoxFrame.bg);
}

void MsgBox::onClick(Object* const target) noexcept {
    MsgButton* btn = nullptr;
    for (MsgButton& b : _btns) {
        if (target == &b) {
            btn = &b;
            break;
        }
    }
    if (btn == nullptr)
        return;
    _ev.action = btn->action();
    Widget::hide(_app.overlay);
    _app.onMsgBox(_ev);
    _app.refreshPage();
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

void MsgBox::formatBody(const char* fmt, va_list ap) noexcept {
    if (fmt == nullptr) {
        _text[0] = '\0';
        return;
    }
    std::vsnprintf(_text, kTextCap, fmt, ap);
    _text[kTextCap - 1u] = '\0';
    clampTextForXstr(_text, kTextCap);
}

void MsgBox::showV(const char* title, const Preset preset, const Action defaultAction, const uint8_t tag,
    const char* fmt, va_list ap) noexcept {
    applyShow(preset, defaultAction, tag);
    setTitle(title != nullptr ? title : kMsgBoxTitles.message);
    formatBody(fmt, ap);
    present();
}

void MsgBox::show(const char* fmt, ...) noexcept {
    va_list ap;
    va_start(ap, fmt);
    showV(kMsgBoxTitles.message, Preset::OK, Action::None, 0u, fmt, ap);
    va_end(ap);
}

void MsgBox::show(const Preset preset, const char* fmt, ...) noexcept {
    va_list ap;
    va_start(ap, fmt);
    showV(kMsgBoxTitles.message, preset, Action::None, 0u, fmt, ap);
    va_end(ap);
}

void MsgBox::show(const char* title, const Preset preset, const uint8_t tag, const Action defaultAction,
    const char* fmt, ...) noexcept {
    va_list ap;
    va_start(ap, fmt);
    showV(title, preset, defaultAction, tag, fmt, ap);
    va_end(ap);
}

void MsgBox::showError(const Preset preset, const uint8_t tag) noexcept {
    showError(preset, defaultForPreset(preset), tag);
}

void MsgBox::showError(const Preset preset, const Action defaultAction, const uint8_t tag) noexcept {
    _ev.route = _app.lastErrorRoute();
    applyShow(preset, defaultAction, tag);
    setTitle(kMsgBoxTitles.error);
    formatStatusMessage(_app.lastError(), _app.lastErrorRoute(), _text, kTextCap);
    clampTextForXstr(_text, kTextCap);
    present();
}

void MsgBox::present() noexcept {
    if (!_routePinned)
        _ev.route = Route{_app.currentPage(), 0u};
    _routePinned = false;

    if (!hasScreen())
        return;

    _ev.action = Action::None;
    Widget::show(_app.overlay, true);
}

} // namespace nex::ovl
