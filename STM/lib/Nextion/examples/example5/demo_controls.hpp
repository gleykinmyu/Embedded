#pragma once

/**
 * MCU API demo for each leaf widget in nexComponents.hpp (no ExComponents).
 *
 * Два режима:
 * - `runAllDemos` — одноразовая настройка (цвета, геометрия, шрифты, enable);
 *   перед каждой страницей — `switchPage` + `pumpUntilIdle`.
 * - `tickLiveDemos` — живые обновления **только на текущей** странице (A→B→C);
 *   `advanceLivePage` — Enter на debug UART, переход к следующей.
 */

#include <cstdint>

#include "nex.hpp"

namespace nex::examples::ex5 {

using namespace nex::comp;

/** Id страниц HMI (совпадают с `PageImpl` в app.hpp). */
inline constexpr uint8_t kPageAId = 0u;
inline constexpr uint8_t kPageBId = 1u;
inline constexpr uint8_t kPageCId = 2u;
inline constexpr uint8_t kPageDId = 3u;

/** Вызывается перед demo* каждого виджета (example5: ожидание Enter на debug UART). */
using BeforeComponentDemoFn = void (*)() noexcept;
inline BeforeComponentDemoFn beforeEachComponentDemo = nullptr;

inline void waitBeforeComponentDemo() noexcept
{
    if (beforeEachComponentDemo != nullptr)
        beforeEachComponentDemo();
}

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

template<BG S>
inline void demoBackground(Styled<S>& w) noexcept
{
    if constexpr (S == BG::Color) {
        w.bg.setColor(kFill);
    } else if constexpr (S == BG::Image) {
        w.bg.setImage(kPic);
    } else if constexpr (S == BG::CropImage) {
        w.bg.setCrop(kPic);
    }
}

template<typename W>
inline void demoStyled(W& w) noexcept
{
    demoDrawable(w);
    demoBackground(w);
}

template<BG S, uint8_t ChannelCount>
inline void demoWfBackground(Waveform<S, ChannelCount>& w) noexcept
{
    if constexpr (S == BG::Color) {
        w.bg.setColor(kFill);
    } else if constexpr (S == BG::Image) {
        w.bg.setImage(kPic);
    } else if constexpr (S == BG::CropImage) {
        w.bg.setCrop(kPic);
    }
}

template<BG S, uint8_t ChannelCount>
inline void demoWaveformDrawable(Waveform<S, ChannelCount>& w) noexcept
{
    demoDrawable(w);
    demoWfBackground(w);
}

inline void demoFont(Printable<BG::Color>& w) noexcept
{
    w.font.setId(1u);
    w.font.setColor(kAccent);
    w.font.setCharSpacing(2u);
}

inline void demoMultiline(Multiline<BG::Color>& w) noexcept
{
    demoFont(w);
    w.setLineSpacing(4u);
    w.setWordWrap(true);
    w.setVAlign(VAlign::Center);
    w.setHAlign(HAlign::Center);
}

template<BG S = BG::Color>
inline void demoPressed(ButtonBase<S>& w) noexcept
{
    if constexpr (S == BG::Color) {
        w.pressed.bg.setColor(Color::std::Maroon);
    } else if constexpr (S == BG::Image) {
        w.pressed.bg.setImage(kPic);
    } else if constexpr (S == BG::CropImage) {
        w.pressed.bg.setCrop(kPic);
    }
    w.pressed.font.setColor(Color::std::White);
}

} // namespace detail

inline void demoTimer(Timer& t) noexcept
{
    NEX_DBG("[ex5a] Timer (тикает на панели после enable)\n");
    t.setPeriod(500u);
    t.enable();
}

inline void demoNumericVariable(NumericVar& v) noexcept
{
    NEX_DBG("[ex5a] NumericVariable\n");
    v.val = 12345;
}

template<uint16_t MaxL = 64u>
inline void demoStringVariable(StringVar<MaxL>& v) noexcept
{
    NEX_DBG("[ex5a] StringVariable\n");
    v.txt.set("MCU str");
}

inline void demoHotspot(Hotspot& h) noexcept
{
    NEX_DBG("[ex5a] Hotspot\n");
    detail::demoTouchArea(h);
}

inline void demoQRCode(QRCode& q) noexcept
{
    NEX_DBG("[ex5a] QRCode\n");
    detail::demoStyled(q);
    q.setPenColor(detail::kMark);
    q.setDataSpacing(2u);
    q.setText("https://nextion.tech");
}

inline void demoPicture(Picture& p) noexcept
{
    NEX_DBG("[ex5a] Picture\n");
    detail::demoDrawable(p);
    p.setImage(detail::kPic);
}

inline void demoCropPicture(CropPicture& c) noexcept
{
    NEX_DBG("[ex5a] CropPicture\n");
    detail::demoDrawable(c);
    c.setCrop(detail::kPic);
}

inline void demoWaveform(Waveform<BG::Color, 4>& w) noexcept
{
    NEX_DBG("[ex5a] Waveform (setup; поток — tickLiveDemos)\n");
    detail::demoWaveformDrawable(w);
    w.ch[0].setColor(Color::std::Red);
    w.ch[1].setColor(Color::std::Green);
    w.ch[2].setColor(Color::std::Blue);
    w.ch[3].setColor(Color::std::White);
    w.bg.setGridColor(0u);
    w.bg.setGridWidth(100u);
    w.bg.setGridHeight(50u);
    w.setDataScale(100u);
}

inline void demoProgressBarColor(ProgressBar<BG::Color>& p) noexcept
{
    NEX_DBG("[ex5b] ProgressBar Color (setup; val — tickLiveDemos)\n");
    detail::demoDrawable(p);
    p.bg.setColor(detail::kFill);
    p.value = 0u;
    p.bar.setColor(Color::std::Green);
    p.bar.setCornerRadius(4u);
}

inline void demoProgressBarImage(ProgressBar<BG::Image>& p) noexcept
{
    NEX_DBG("[ex5b] ProgressBar Image (setup; val — tickLiveDemos)\n");
    detail::demoDrawable(p);
    p.value = 0u;
    p.bg.setImage(detail::kPic);
    p.bar.setImage(detail::kPic);
}

inline void demoSlider(Slider<>& s) noexcept
{
    NEX_DBG("[ex5b] Slider (setup; val — tickLiveDemos)\n");
    detail::demoStyled(s);
    s.value = 0u;
    s.cursor.setWidth(20u);
    s.cursor.setHeight(30u);
    s.cursor.setColor(detail::kAccent);
    s.bg2.setColor(Color::std::Gray);
}

inline void demoGauge(Gauge<>& g) noexcept
{
    NEX_DBG("[ex5b] Gauge (setup; angle — tickLiveDemos)\n");
    detail::demoStyled(g);
    g.setAngle(0);
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

inline void demoComboBox(ComboBox<>& c) noexcept
{
    NEX_DBG("[ex5b] ComboBox\n");
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

inline void demoTextSelect(TextSelect<>& t) noexcept
{
    NEX_DBG("[ex5b] TextSelect\n");
    detail::demoFont(t);
    t.path.set("sd0:/list.txt");
    t.val = 1;
    t.setCellSize(24u);
    t.setSelColor(Color::std::Yellow);
    t.setLineColor(Color::std::Cyan);
    t.setSelectionLine(true);
}

inline void demoDataRecord(DataRecord<>& d) noexcept
{
    NEX_DBG("[ex5b] DataRecord\n");
    detail::demoFont(d);
    d.path.set("sd0:/data.csv");
    d.setRecordLength(64u);
    d.setFormat(NumFormat::Dec);
    d.setMode(0u);
    d.setOrder(0u);
    d.setCellSize(24u);
    d.setGridColor(Color::std::Gray);
    d.setGridWidth(10u);
    d.setGridHeight(20u);
    d.setCellBgColor(Color::std::Black);
    d.setCellColor(Color::std::Silver);
    d.setHAlign(HAlign::Center);
    d.left = 0u;
    d.ch = 0u;
    d.val = 0;
}

inline void demoTextual(Textual<>& t) noexcept
{
    detail::demoMultiline(t);
    t.setText("Hello Text");
}

inline void demoText(Text<>& t) noexcept
{
    NEX_DBG("[ex5c] Text\n");
    demoTextual(t);
    t.disablePassword();
}

inline void demoSlidingText(SlidingText<>& s) noexcept
{
    NEX_DBG("[ex5c] SlidingText\n");
    // TODO: demoTextual → demoMultiline вызывает setVAlign(ycen); у SlidingText атрибута нет — отдельный demo без ycen.
    demoTextual(s);
    s.setShowProgressBar(nex::ShowProgressBar::OperationTime);
    s.val_y = 0u;
}

inline void demoScrollText(ScrollText<>& s) noexcept
{
    NEX_DBG("[ex5c] ScrollText (прокрутка на панели после enable)\n");
    demoTextual(s);
    s.setText("Scrolling marquee text — driven by panel timer");
    s.setScrollDirection(ScrollDirection::LeftToRight);
    s.setScrollStep(4u);
    s.setPeriod(200u);
    s.enable();
}

inline void demoNumeric(Numeric<>& n) noexcept
{
    NEX_DBG("[ex5c] Numeric\n");
    detail::demoMultiline(n);
    n.val = 42;
}

inline void demoNumber(Number<>& n) noexcept
{
    NEX_DBG("[ex5c] Number\n");
    demoNumeric(n);
    n.setDigitCount(4u);
    n.setFormat(NumFormat::Dec);
}

inline void demoXFloat(XFloat<>& x) noexcept
{
    NEX_DBG("[ex5c] XFloat\n");
    demoNumeric(x);
    x.setFormat(3u, 2u);
}

inline void demoSelection(Selection& s) noexcept
{
    detail::demoStyled(s);
    s.setMarkerColor(detail::kMark);
    s.val = true;
}

inline void demoButton(Button<>& b, Drawable& layerRef) noexcept
{
    NEX_DBG("[ex5d] Button\n");
    demoTextual(b);
    detail::demoPressed(b);
    b.placeAbove(layerRef);
    b.move(Point{0, 0}, Point{4, 4}, 0u, 0u);
}

inline void demoDualStateButton(DualStateButton<>& d) noexcept
{
    NEX_DBG("[ex5d] DualStateButton\n");
    demoTextual(d);
    detail::demoPressed(d);
    d.val = 1u;
}

inline void demoCheckbox(Checkbox& c) noexcept
{
    NEX_DBG("[ex5d] Checkbox\n");
    demoSelection(c);
}

inline void demoRadio(Radio& r) noexcept
{
    NEX_DBG("[ex5d] Radio\n");
    demoSelection(r);
}

inline void demoToggleSwitch(ToggleSwitch& t) noexcept
{
    NEX_DBG("[ex5d] ToggleSwitch\n");
    demoSelection(t);
    t.pressed.bg.setColor(Color::std::Gray);
    t.pressed.setMarkerColor(Color::std::Silver);
    t.font.setId(1u);
    t.font.setColor(Color::std::Green);
    t.setLabelGap(4u);
    t.txt.set("ON");
}

struct PageAWidgets {
    Timer& timer;
    NumericVar& nvar;
    StringVar<64>& svar;
    Hotspot& hotspot;
    QRCode& qrcode;
    Picture& picture;
    CropPicture& crop_picture;
    Waveform<BG::Color, 4>& waveform;
};

struct PageBWidgets {
    ProgressBar<BG::Color>& pbar_color;
    ProgressBar<BG::Image>& pbar_image;
    Slider<>& slider;
    Gauge<>& gauge;
    ComboBox<>& combo;
    TextSelect<>& text_select;
    DataRecord<>& data_record;
};

struct PageCWidgets {
    Text<>& text;
    SlidingText<>& sltext;
    ScrollText<>& scroll_text;
    Number<>& number;
    XFloat<>& xfloat;
};

struct PageDWidgets {
    Button<>& button;
    DualStateButton<>& dual_button;
    Checkbox& checkbox;
    Radio& radio;
    ToggleSwitch& toggle;
};

inline void runPageADemos(PageAWidgets& w) noexcept
{
    waitBeforeComponentDemo();
    demoTimer(w.timer);
    waitBeforeComponentDemo();
    demoNumericVariable(w.nvar);
    waitBeforeComponentDemo();
    demoStringVariable(w.svar);
    waitBeforeComponentDemo();
    demoHotspot(w.hotspot);
    waitBeforeComponentDemo();
    demoQRCode(w.qrcode);
    waitBeforeComponentDemo();
    demoPicture(w.picture);
    waitBeforeComponentDemo();
    demoCropPicture(w.crop_picture);
    waitBeforeComponentDemo();
    demoWaveform(w.waveform);
}

inline void runPageBDemos(PageBWidgets& w) noexcept
{
    waitBeforeComponentDemo();
    demoProgressBarColor(w.pbar_color);
    waitBeforeComponentDemo();
    demoProgressBarImage(w.pbar_image);
    waitBeforeComponentDemo();
    demoSlider(w.slider);
    waitBeforeComponentDemo();
    demoGauge(w.gauge);
    waitBeforeComponentDemo();
    demoComboBox(w.combo);
    waitBeforeComponentDemo();
    demoTextSelect(w.text_select);
    //waitBeforeComponentDemo();
    //demoDataRecord(w.data_record);
}

inline void runPageCDemos(PageCWidgets& w) noexcept
{
    waitBeforeComponentDemo();
    demoText(w.text);
    waitBeforeComponentDemo();
    demoSlidingText(w.sltext);
    waitBeforeComponentDemo();
    demoScrollText(w.scroll_text);
    waitBeforeComponentDemo();
    demoNumber(w.number);
    waitBeforeComponentDemo();
    demoXFloat(w.xfloat);
}

inline void runPageDDemos(PageDWidgets& w) noexcept
{
    waitBeforeComponentDemo();
    demoButton(w.button, w.button);
    waitBeforeComponentDemo();
    demoDualStateButton(w.dual_button);
    waitBeforeComponentDemo();
    demoCheckbox(w.checkbox);
    waitBeforeComponentDemo();
    demoRadio(w.radio);
    waitBeforeComponentDemo();
    demoToggleSwitch(w.toggle);
}

inline void runAllDemos(Application& app, PageAWidgets& a, PageBWidgets& b, PageCWidgets& c,
    PageDWidgets& d) noexcept
{
    NEX_DBG("[ex5] === attribute demo start ===\n");

    app.switchPage(kPageAId);
    app.pumpUntilIdle();
    runPageADemos(a);
    app.pumpUntilIdle();

    waitBeforeComponentDemo();
    app.switchPage(kPageBId);
    app.pumpUntilIdle();
    runPageBDemos(b);
    app.pumpUntilIdle();

    waitBeforeComponentDemo();
    app.switchPage(kPageCId);
    app.pumpUntilIdle();
    runPageCDemos(c);
    app.pumpUntilIdle();

    waitBeforeComponentDemo();
    app.switchPage(kPageDId);
    app.pumpUntilIdle();
    runPageDDemos(d);
    app.pumpUntilIdle();

    waitBeforeComponentDemo();
    app.switchPage(kPageAId);
    app.pumpUntilIdle();

    NEX_DBG("[ex5] === attribute demo enqueued ===\n");
}

/** Состояние периодического live-demo (после `runAllDemos`). */
struct LiveDemoState {
    uint32_t last_ms = 0;
    uint8_t phase = 0;
    /** 0=ex5a, 1=ex5b, 2=ex5c — страницы с живым обновлением. */
    uint8_t live_page = 0;
};

inline constexpr uint8_t kLivePageCount = 3u;

inline constexpr uint32_t kLiveDemoPeriodMs = 400u;

[[nodiscard]] inline uint8_t pageIdForLiveSlot(uint8_t live_page) noexcept
{
    switch (live_page % kLivePageCount) {
    default:
    case 0u: return kPageAId;
    case 1u: return kPageBId;
    case 2u: return kPageCId;
    }
}

[[nodiscard]] inline const char* livePageName(uint8_t live_page) noexcept
{
    switch (live_page % kLivePageCount) {
    default:
    case 0u: return "ex5a";
    case 1u: return "ex5b";
    case 2u: return "ex5c";
    }
}

inline void logLivePagePrompt(uint8_t live_page) noexcept
{
    NEX_DBG("[ex5] live demo: %s — Enter — следующая страница (reboot — сброс MCU)\n",
        livePageName(live_page));
}

/** Enter в main loop: следующая live-страница (A→B→C→A…). */
inline void advanceLivePage(Application& app, LiveDemoState& st) noexcept
{
    st.live_page = static_cast<uint8_t>((st.live_page + 1u) % kLivePageCount);
    st.phase = 0;
    st.last_ms = 0;
    app.switchPage(pageIdForLiveSlot(st.live_page));
    logLivePagePrompt(st.live_page);
}

inline void tickLivePageA(PageAWidgets& a, uint8_t t) noexcept
{
    const uint8_t wave0 = static_cast<uint8_t>(128 + static_cast<int8_t>(t - 128));
    const uint8_t wave1 = static_cast<uint8_t>(255u - wave0);
    if ((t & 1u) == 0u)
        a.waveform.ch[0].add(wave0);
    else
        a.waveform.ch[1].add(wave1);
    a.nvar.val = static_cast<int32_t>(t);
}

inline void tickLivePageB(PageBWidgets& b, uint8_t t) noexcept
{
    const uint8_t level = static_cast<uint8_t>((static_cast<uint16_t>(t) * 100u) / 255u);
    b.pbar_color.value = level;
    b.pbar_image.value = static_cast<uint8_t>(100u - level);
    b.gauge.setAngle(static_cast<uint16_t>((static_cast<uint16_t>(t) * 360u) / 255u));
    b.slider.value = level;
}

inline void tickLivePageC(PageCWidgets& c, uint8_t t) noexcept
{
    c.number.val = static_cast<int32_t>(t);
    c.xfloat.val = static_cast<int32_t>(t);
    c.sltext.val_y = static_cast<uint16_t>(t);
}

/** Обновление виджетов на **текущей** live-странице (`st.live_page`). */
inline void tickLiveDemos(Application& app, LiveDemoState& st, uint32_t now_ms, PageAWidgets& a,
    PageBWidgets& b, PageCWidgets& c) noexcept
{
    if (st.last_ms != 0u && (now_ms - st.last_ms) < kLiveDemoPeriodMs)
        return;
    st.last_ms = now_ms;

    const uint8_t t = st.phase;
    ++st.phase;

    switch (st.live_page % kLivePageCount) {
    case 0u:
        tickLivePageA(a, t);
        break;
    case 1u:
        tickLivePageB(b, t);
        break;
    default:
        tickLivePageC(c, t);
        break;
    }
}

} // namespace nex::examples::ex5
