#pragma once

/**
 * MCU API demo (setters, enable/disable, attr::) for each ex5 leaf widget.
 * Called from page onLoad after UART is ready.
 */

#include <cstdint>

#include "nex.hpp"

namespace nex::examples::ex5 {

using namespace nex::comp;

namespace detail {

constexpr Color kFill = Color::std::Navy;
constexpr Color kAccent = Color::std::Cyan;
constexpr Color kMark = Color::std::Yellow;
constexpr PicId kPic = 1u;

inline void demoTouchArea(TouchArea& w) noexcept
{
#if NEX_TOUCH_AREA_POSITION
    w.x = 10u;
    w.y = 20u;
#endif
    w.touchSwitch(true);
}

inline void demoDrawable(Drawable& w) noexcept
{
    demoTouchArea(w);
#if NEX_DRAWABLE_DRAG
    w.setDraggable(true);
#endif
#if NEX_DRAWABLE_OPACITY
    w.setOpacity(100u);
#endif
#if NEX_DRAWABLE_EFFECT
    w.setTransitionEffect(0u);
#endif
    w.show();
    w.refresh();
}

template<BGStyle S>
inline void demoBackground(Styled<S>& w) noexcept
{
    if constexpr (S == BGStyle::Color) {
        w.bg.setColor(kFill);
    } else if constexpr (S == BGStyle::Image) {
        w.bg.setImage(kPic);
    } else if constexpr (S == BGStyle::CropImage) {
        w.bg.setCrop(kPic);
    }
}

template<typename W>
inline void demoStyled(W& w) noexcept
{
    demoDrawable(w);
    demoBackground(w);
}

template<BGStyle S, uint8_t ChannelCount>
inline void demoWfBackground(Waveform<S, ChannelCount>& w) noexcept
{
    if constexpr (S == BGStyle::Color) {
        w.wfBackground.setColor(kFill);
    } else if constexpr (S == BGStyle::Image) {
        w.wfBackground.setImage(kPic);
    } else if constexpr (S == BGStyle::CropImage) {
        w.wfBackground.setCrop(kPic);
    }
}

template<BGStyle S, uint8_t ChannelCount>
inline void demoWaveformDrawable(Waveform<S, ChannelCount>& w) noexcept
{
    demoDrawable(w);
    demoWfBackground(w);
}

inline void demoFont(Printable<BGStyle::Color>& w) noexcept
{
    w.font.setId(1u);
    w.font.setColor(kAccent);
    w.font.setCharSpacing(2u);
}

inline void demoMultiline(Multiline<BGStyle::Color>& w) noexcept
{
    demoFont(w);
    w.setLineSpacing(4u);
    w.setWordWrap(true);
    w.setVAlign(VAlign::Center);
    w.setHAlign(HAlign::Center);
}

template<BGStyle S = BGStyle::Color, uint16_t TxtMaxL = 256u>
inline void demoPressed(ButtonLikeComponent<S, TxtMaxL>& w) noexcept
{
    if constexpr (S == BGStyle::Color) {
        w.pressed.bg.setColor(Color::std::Maroon);
    } else if constexpr (S == BGStyle::Image) {
        w.pressed.bg.setImage(detail::kPic);
    } else if constexpr (S == BGStyle::CropImage) {
        w.pressed.bg.setCrop(detail::kPic);
    }
    w.pressed.font.setColor(Color::std::White);
}

} // namespace detail

inline void demoTimer(Timer& t) noexcept
{
    NEX_DBG("[ex5] Timer\n");
    t.setPeriod(500u);
    t.enable();
}

inline void demoNumericVariable(NumericVar& v) noexcept
{
    NEX_DBG("[ex5] NumericVariable\n");
    v.val = 12345;
}

template<uint16_t MaxL = 64u>
inline void demoStringVariable(StringVar<MaxL>& v) noexcept
{
    NEX_DBG("[ex5] StringVariable\n");
    v.txt.set("MCU str");
}

inline void demoHotspot(Hotspot& h) noexcept
{
    NEX_DBG("[ex5] Hotspot\n");
    detail::demoTouchArea(h);
}

inline void demoQRCode(QRCode& q) noexcept
{
    NEX_DBG("[ex5] QRCode\n");
    detail::demoStyled(q);
    q.setPco(detail::kMark);
    q.setScale(2u);
    q.setText("https://nextion.tech");
}

inline void demoPicture(Picture& p) noexcept
{
    NEX_DBG("[ex5] Picture\n");
    detail::demoStyled(p);
}

inline void demoCropPicture(CropPicture& c) noexcept
{
    NEX_DBG("[ex5] CropPicture\n");
    detail::demoStyled(c);
    c.setCrop(detail::kPic);
}

inline void demoWaveform(Waveform<BGStyle::Color, 4>& w) noexcept
{
    NEX_DBG("[ex5] Waveform\n");
    detail::demoWaveformDrawable(w);
    w.ch[0].setColor(Color::std::Red);
    w.ch[1].setColor(Color::std::Green);
    w.ch[2].setColor(Color::std::Blue);
    w.ch[3].setColor(Color::std::White);
    w.ch[0].add(128u);
    w.wfBackground.setGridColor(0u);
    w.wfBackground.setGridWidth(100u);
    w.wfBackground.setGridHeight(50u);
    w.setDataScale(100u);
}

inline void demoProgressBarColor(ProgressBar<BGStyle::Color>& p) noexcept
{
    NEX_DBG("[ex5] ProgressBar Color\n");
    detail::demoDrawable(p);
    p.bg.setColor(detail::kFill);
    p.value = 60u;
    p.bar.setColor(Color::std::Green);
    p.bar.setCornerRadius(4u);
}

inline void demoProgressBarImage(ProgressBar<BGStyle::Image>& p) noexcept
{
    NEX_DBG("[ex5] ProgressBar Image\n");
    detail::demoDrawable(p);
    p.value = 40u;
    p.bg.setImage(detail::kPic);
    p.bar.setImage(detail::kPic);
}

inline void demoSlider(Slider<>& s) noexcept
{
    NEX_DBG("[ex5] Slider\n");
    detail::demoStyled(s);
    s.value = 50u;
    s.cursor.setWidth(20u);
    s.cursor.setHeight(30u);
    s.cursor.setThumbColor(detail::kAccent);
    s.bg2.setColor(Color::std::Gray);
}

inline void demoGauge(Gauge<>& g) noexcept
{
    NEX_DBG("[ex5] Gauge\n");
    detail::demoStyled(g);
    g.setAngle(75);
    g.center.setColor(detail::kMark);
    g.center.setOffset(0u);
    g.center.setDiameter(50u);
    g.pointer.setColor(detail::kAccent);
    g.pointer.setHeadLength(20);
    g.pointer.setFootLength(8);
    g.pointer.setHeadWidth(6u);
    g.pointer.setBodyWidth(4u);
    g.pointer.setFootWidth(10u);
}

inline void demoListSelect(ListSelect<>& ls) noexcept
{
    NEX_DBG("[ex5] ListSelect (ComboBox base)\n");
    detail::demoFont(ls);
    ls.path.set("sd0:/list.txt");
    ls.val = 1;
    ls.setCellSize(24u);
}

inline void demoComboBox(ComboBox<>& c) noexcept
{
    NEX_DBG("[ex5] ComboBox\n");
    detail::demoFont(c);
    c.path.set("sd0:/list.txt");
    c.val = 1;
    c.setVAlign(VAlign::Center);
    c.setHAlign(HAlign::Center);
    c.border.setColor(detail::kAccent);
    c.border.setWidth(2u);
    c.arrow.show();
    c.arrow.setColor(Color::std::White);
    c.cells.setBgColor(Color::std::Black);
    c.cells.setColor(Color::std::Silver);
    c.cells.setSelBgColor(Color::std::Blue);
    c.cells.setSelColor(Color::std::Yellow);
    c.cells.setExpandDirection(ComboExpandDirection::Down);
    c.cells.setExpandCount(8u);
    c.cells.setSpacing(0);
    c.cells.setCornerRadius(4u);
    c.setCellSize(24u);
    c.cells.setMarker(ComboMarker::Triangle);
    c.cells.setMarkerSize(12u);
    c.cells.setMarkerSpacing(0);
}

inline void demoTextComponent(TextComponent<>& t) noexcept
{
    detail::demoMultiline(t);
    t.txt.set("Hello Text");
}

inline void demoText(Text<>& t) noexcept
{
    NEX_DBG("[ex5] Text\n");
    demoTextComponent(t);
    t.disablePassword();
}

inline void demoScrollText(ScrollText<>& s) noexcept
{
    NEX_DBG("[ex5] ScrollText\n");
    demoTextComponent(s);
    s.setScrollDirection(ScrollDirection::LeftToRight);
    s.setScrollStep(4u);
    s.setPeriod(200u);
    s.enable();
}

inline void demoButton(Button<>& b, Drawable& layerRef) noexcept
{
    NEX_DBG("[ex5] Button\n");
    demoTextComponent(b);
    detail::demoPressed(b);
    b.placeAbove(layerRef);
    b.move(Point{0, 0}, Point{4, 4}, 0u, 0u);
}

inline void demoDualStateButton(DualStateButton<>& d) noexcept
{
    NEX_DBG("[ex5] DualStateButton\n");
    demoTextComponent(d);
    detail::demoPressed(d);
    d.val = 1u;
}

inline void demoNumeric(Numeric<>& n) noexcept
{
    NEX_DBG("[ex5] Numeric\n");
    detail::demoMultiline(n);
    n.val = 42;
}

inline void demoNumber(Number<>& n) noexcept
{
    NEX_DBG("[ex5] Number\n");
    demoNumeric(n);
    n.setDigitCount(4u);
    n.setFormat(NumFormat::Dec);
}

inline void demoXFloat(XFloat<>& x) noexcept
{
    NEX_DBG("[ex5] XFloat\n");
    demoNumeric(x);
    x.setFormat(3u, 2u);
}

inline void demoSelection(Selection<>& s) noexcept
{
    detail::demoStyled(s);
    s.setMarkerColor(detail::kMark);
    s.val = true;
}

inline void demoCheckbox(Checkbox<>& c) noexcept
{
    NEX_DBG("[ex5] Checkbox\n");
    demoSelection(c);
}

inline void demoRadio(Radio<>& r) noexcept
{
    NEX_DBG("[ex5] Radio\n");
    demoSelection(r);
}

inline void demoToggleSwitch(ToggleSwitch<>& t) noexcept
{
    NEX_DBG("[ex5] ToggleSwitch\n");
    demoSelection(t);
    t.pressed.bg.setColor(Color::std::Gray);
    t.pressed.setMarkerColor(Color::std::Silver);
    t.font.setId(1u);
    t.font.setColor(Color::std::Green);
    t.setLabelGap(4u);
    t.txt.set("ON");
}

} // namespace nex::examples::ex5
