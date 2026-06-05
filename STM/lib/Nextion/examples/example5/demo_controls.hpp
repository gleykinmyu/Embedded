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
    w.enableWordWrap();
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
    w.pressed.font.setPressedTextColor(Color::std::White);
}

} // namespace detail

inline void demoTimer(Timer& t) noexcept
{
    NEX_DBG("[ex5] Timer\n");
    t.setPeriod(500u);
    t.enable();
}

inline void demoNumericVariable(NumericVariable& v) noexcept
{
    NEX_DBG("[ex5] NumericVariable\n");
    v.val = 12345;
}

template<uint16_t MaxL = 64u>
inline void demoStringVariable(StringVariable<MaxL>& v) noexcept
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
    q.txt.set("https://nextion.tech");
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

inline void demoWaveform(Waveform<>& w) noexcept
{
    NEX_DBG("[ex5] Waveform\n");
    detail::demoStyled(w);
    w.setChannel(0u);
    w.setPco0(Color::std::Red);
    w.setPco1(Color::std::Green);
    w.setPco2(Color::std::Blue);
    w.setPco3(Color::std::White);
    w.setGdc(0u);
    w.setGdw(100u);
    w.setGdh(50u);
    w.setDis(0u);
    w.setPlotWidth(200u);
    w.setPlotHeight(80u);
}

inline void demoProgressBarColor(ProgressBar<BGStyle::Color>& p) noexcept
{
    NEX_DBG("[ex5] ProgressBar Color\n");
    detail::demoStyled(p);
    p.value = 60u;
    p.setBarColor(Color::std::Green);
    p.setCornerRadius(4u);
}

inline void demoProgressBarImage(ProgressBar<BGStyle::Image>& p) noexcept
{
    NEX_DBG("[ex5] ProgressBar Image\n");
    detail::demoDrawable(p);
    p.setValue(40u);
    p.bg.setImage(detail::kPic);
    p.setPpic(detail::kPic);
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
    g.setVal(75);
    g.format.set("%d");
    g.setUpImage(detail::kPic);
    g.setDownImage(detail::kPic);
    g.setLeftImage(detail::kPic);
    g.setScaleColor(detail::kAccent);
    g.setAccentColor(detail::kMark);
    g.setScaleHeight(12u);
    g.vvs0.set("min");
    g.vvs1.set("mid");
    g.vvs2.set("max");
}

inline void demoListSelect(ListSelect<>& ls) noexcept
{
    NEX_DBG("[ex5] ListSelect (ComboBox base)\n");
    detail::demoFont(ls);
    ls.path.set("sd0:/list.txt");
    ls.val = 1;
    ls.ch = 0u;
    ls.setItemSpacing(2u);
    ls.setRowHeight(24u);
}

inline void demoComboBox(ComboBox<>& c) noexcept
{
    NEX_DBG("[ex5] ComboBox\n");
    demoListSelect(c);
    c.enableVerticalCenter();
    c.setUpImage(detail::kPic);
    c.setPco3(Color::std::White);
    c.setBco1(Color::std::Black);
    c.setPco1(Color::std::Silver);
    c.list_dir = 0u;
    c.setRowCount(8u);
    c.setListOffset(0);
    c.setBco2(Color::std::Blue);
    c.setPco2(Color::std::Yellow);
    c.setDownImage(detail::kPic);
    c.setListMode(0u);
    c.setDropDownWidth(120u);
    c.setListOffset2(0);
    c.enableHorizontalCenter();
    c.txt.set("Item 1");
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
    t.bindKeyboard();
    t.disablePassword();
}

inline void demoScrollText(ScrollText<>& s) noexcept
{
    NEX_DBG("[ex5] ScrollText\n");
    demoTextComponent(s);
    s.bindKeyboard();
    s.dir = 0u;
    s.setScrollStep(4u);
    s.setScrollPeriod(200u);
    s.enableAutoScroll();
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
    n.setFormat(NumFormat::Dec);
}

inline void demoNumber(Number<>& n) noexcept
{
    NEX_DBG("[ex5] Number\n");
    demoNumeric(n);
    n.setDigitCount(4u);
}

inline void demoXFloat(XFloat<>& x) noexcept
{
    NEX_DBG("[ex5] XFloat\n");
    demoNumeric(x);
    x.point.setDigitsBeforePoint(3u);
    x.point.setDigitsAfterPoint(2u);
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
    t.setTrackColor(Color::std::Gray);
    t.setOffColor(Color::std::Silver);
    t.setOnColor(Color::std::Green);
    t.setLabelFontId(1u);
    t.setLabelGap(4u);
    t.txt.set("ON");
}

} // namespace nex::examples::ex5
