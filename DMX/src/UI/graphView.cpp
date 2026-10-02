#include "UI/graphView.hpp"

#include "UI/application.hpp"
#include "UI/enc.hpp"
#include "board.hpp"
#include "model/tester.hpp"

#include <cstdio>
#include <cstring>

namespace ui {

namespace {

constexpr nex::FontId kFont = 0u;
constexpr nex::Coord kAxisW = 40;
constexpr nex::Coord kTimeH = 16;
constexpr nex::Coord kSpanH = layout::kBtnH;
constexpr uint8_t kLegCols = 8u;
constexpr nex::Coord kLegRowH = 22;
constexpr nex::Coord kLegX0 = 4;
constexpr nex::Coord kLegH = kLegRowH;
constexpr nex::Coord kLegAvailW = static_cast<nex::Coord>(layout::kScreenW - kLegX0 - layout::kPad);
constexpr nex::Coord kLegItemW = static_cast<nex::Coord>(kLegAvailW / static_cast<nex::Coord>(kLegCols));
constexpr nex::Coord kPlotTop = kLegH;
constexpr nex::Coord kPlotGap = 3;
constexpr uint8_t kGridX = 10u;
constexpr uint8_t kGridY = 4u;
constexpr uint8_t kTimeLabelEvery = 2u;

[[nodiscard]] nex::Coord gridAt(const nex::Coord origin, const nex::Coord span, const uint8_t i, const uint8_t n) noexcept
{
    if (n == 0u)
        return origin;
    return static_cast<nex::Coord>(static_cast<int32_t>(origin) + static_cast<int32_t>(i) * (span - 1) / n);
}

[[nodiscard]] nex::Region plotRect(const nex::Region& g) noexcept
{
    const nex::Coord top = static_cast<nex::Coord>(kPlotTop + kPlotGap);
    const nex::Coord availW = static_cast<nex::Coord>(g.size.w - kAxisW - 4);
    const nex::Coord availH = static_cast<nex::Coord>(g.size.h - top - kTimeH - kPlotGap);
    const int32_t sx = (availW > 1) ? (static_cast<int32_t>(availW) - 1) / kGridX : 1;
    const int32_t sy = (availH > 1) ? (static_cast<int32_t>(availH) - 1) / kGridY : 1;
    const nex::Coord pw = static_cast<nex::Coord>(sx * kGridX + 1);
    const nex::Coord ph = static_cast<nex::Coord>(sy * kGridY + 1);
    const nex::Coord x = static_cast<nex::Coord>(g.ul.x + kAxisW);
    const nex::Coord y = static_cast<nex::Coord>(g.ul.y + top);
    return nex::Region(nex::Point{x, y}, nex::Rect{pw, ph});
}

[[nodiscard]] nex::Coord xAtSample(const nex::Region& plot, const uint16_t i) noexcept
{
    const uint16_t use = (kTraceLen > 1u) ? static_cast<uint16_t>(kTraceLen - 1u) : 1u;
    const uint16_t clamped = (i >= kTraceLen) ? use : i;
    return static_cast<nex::Coord>(
        static_cast<int32_t>(plot.ul.x) + static_cast<int32_t>(clamped) * (plot.size.w - 1) / use);
}

[[nodiscard]] nex::Coord yAtValue(
    const nex::Region& plot, const uint8_t v, const uint8_t lo, const uint8_t hi) noexcept
{
    uint8_t top = hi;
    if (top <= lo)
        top = static_cast<uint8_t>(lo + 1u);
    int32_t clipped = v;
    if (clipped < lo)
        clipped = lo;
    if (clipped > top)
        clipped = top;
    return static_cast<nex::Coord>(static_cast<int32_t>(plot.ul.y + plot.size.h - 1)
        - (clipped - lo) * (plot.size.h - 1) / (top - lo));
}

void copyText(char* dst, std::size_t cap, const char* src) noexcept
{
    if (dst == nullptr || cap == 0u)
        return;
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    std::strncpy(dst, src, cap - 1u);
    dst[cap - 1u] = '\0';
}

} // namespace

GraphView::GraphView(Application& app) noexcept
    : _app(app)
    , _back{"сетка", nex::Rect{layout::kActionW, layout::kBtnH}, kBtnIdle}
    , _spanPrev{"<", nex::Rect{layout::kNavW, kSpanH}, kBtnIdle}
    , _spanNext{">", nex::Rect{layout::kNavW, kSpanH}, kBtnIdle}
    , _yBandPrev{"<", nex::Rect{layout::kNavW, kSpanH}, kBtnDisabled}
    , _yBandNext{">", nex::Rect{layout::kNavW, kSpanH}, kBtnIdle}
    , _scale{"DMX", nex::Rect{layout::kScaleW, layout::kBtnH}, kBtnIdle}
{
    setRegion(nex::Region(nex::Point{0, 0}, nex::Rect{layout::kScreenW, layout::kScreenH}));
    _plot.host = this;
    _link.align = nex::HAlign::Left;
    _spanLabel.align = nex::HAlign::Center;
    _yBandLabel.align = nex::HAlign::Center;
    _link.fg = kOk;

    addChildTop(_plot);
    addChildTop(_spanPrev);
    addChildTop(_spanLabel);
    addChildTop(_spanNext);
    addChildTop(_yBandPrev);
    addChildTop(_yBandLabel);
    addChildTop(_yBandNext);
    addChildTop(_scale);
    addChildTop(_back);
    addChildTop(_pip);
    addChildTop(_link);
    layout();
}

void GraphView::applyOem() noexcept
{
    if (_oemReady)
        return;
    enc::utf8ToOem(_oemBack, sizeof(_oemBack), "сетка");
    RadioGroup::setLabel(_back, _oemBack);
    _oemReady = true;
}

void GraphView::showOn(nex::ovl::Overlay& ovl) noexcept
{
    applyOem();
    _overlay = &ovl;
    if (_tester != nullptr)
        _tester->clearTrace();
    layout();
    syncChrome();
    show(ovl);
    _link.dirty = true;
    _pip.dirty = true;
    presentDirtyLabels();
}

void GraphView::hideFrom(nex::ovl::Overlay& ovl) noexcept
{
    hide(ovl);
    if (_overlay == &ovl)
        _overlay = nullptr;
}

void GraphView::refresh() noexcept
{
    if (_overlay == nullptr || !isVisible())
        return;
    syncChrome();
    presentDirtyLabels();
    _plot.presentNew();
}

void GraphView::layout() noexcept
{
    const nex::Coord fy = layout::graphFootY();
    _plot.setRegion(nex::Region(
        nex::Point{0, 0},
        nex::Rect{layout::kScreenW, fy}));
    Widget::layout();
    layoutChrome();
}

void GraphView::layoutChrome() noexcept
{
    const nex::Coord kH = layout::kBtnH;
    const nex::Coord y2 = layout::statusRowY();
    const nex::Coord pipX = layout::kPad;
    const nex::Coord linkX = static_cast<nex::Coord>(pipX + layout::kPipW + layout::kGap);
    _pip.setRegion(nex::Region(nex::Point{pipX, y2}, nex::Rect{layout::kPipW, kH}));
    const nex::Coord spanX = layout::graphSpanPrevX();
    const nex::Coord linkW = static_cast<nex::Coord>(spanX - linkX);
    _link.setRegion(nex::Region(nex::Point{linkX, y2}, nex::Rect{linkW, kH}));
    _scale.setRegion(nex::Region(nex::Point{layout::graphScaleX(), y2}, nex::Rect{layout::kScaleW, kH}));
    _back.setRegion(nex::Region(nex::Point{layout::actionX(), y2}, nex::Rect{layout::kActionW, kH}));

    const auto placeArrows = [y2, kH](nex::ovl::Button& prev, Label& mid, nex::ovl::Button& next,
        const nex::Coord x, const nex::Coord labelW) noexcept {
        prev.setRegion(nex::Region(nex::Point{x, y2}, nex::Rect{layout::kNavW, kH}));
        const nex::Coord lx = static_cast<nex::Coord>(x + layout::kNavW + layout::kArrowGap);
        mid.setRegion(nex::Region(nex::Point{lx, y2}, nex::Rect{labelW, kH}));
        next.setRegion(nex::Region(
            nex::Point{static_cast<nex::Coord>(lx + labelW + layout::kArrowGap), y2},
            nex::Rect{layout::kNavW, kH}));
    };
    placeArrows(_spanPrev, _spanLabel, _spanNext, layout::graphSpanPrevX(), layout::kSpanLabelW);
    placeArrows(_yBandPrev, _yBandLabel, _yBandNext, layout::graphYBandPrevX(), layout::kYBandW);
}

void GraphView::syncChrome() noexcept
{
    LinkSnap snap{};
    if (_tester != nullptr) {
        _tester->pollLink(boardClockMs());
        snap = _tester->linkSnap();
    }

    char buf[36]{};
    enc::utf8ToOem(buf, sizeof(buf), linkCaptionUtf8(snap.state));
    _link.setText(buf);
    _link.setFg(snap.state == LinkUi::Live ? kOk : kErr);
    _pip.setFill(snap.state == LinkUi::Error ? kErr : (snap.pipOn ? kMain : kBorder));
    syncSpanLabel();
    syncYBandLabel();
    syncScaleLabel();
}

void GraphView::syncYBandLabel() noexcept
{
    const YBand band = (_tester != nullptr) ? _tester->yBand() : YBand::Full;
    char next[sizeof(_yBandText)]{};
    if (_tester != nullptr)
        _tester->formatYBand(next, sizeof(next));
    else
        copyText(next, sizeof(next), "0-255");
    if (band != _bandShown || std::strcmp(_yBandText, next) != 0) {
        _bandShown = band;
        std::memcpy(_yBandText, next, sizeof(next));
        _yBandLabel.setText(_yBandText);
    }

    const bool canLo = static_cast<uint8_t>(band) > 0u;
    const bool canHi = static_cast<uint8_t>(band) + 1u < kYBandN;
    if (canLo != _yBandPrevOn) {
        _yBandPrevOn = canLo;
        styleSpan(_yBandPrev, canLo);
        present(_yBandPrev);
    }
    if (canHi != _yBandNextOn) {
        _yBandNextOn = canHi;
        styleSpan(_yBandNext, canHi);
        present(_yBandNext);
    }
}

void GraphView::syncScaleLabel() noexcept
{
    const ValueScale scale = (_tester != nullptr) ? _tester->scale() : ValueScale::Dmx;
    if (scale == _scaleShown && _oemReady)
        return;
    _scaleShown = scale;
    const nex::Region keep = _scale.region();
    _scale.setLabel(scale == ValueScale::Percent ? "%" : "DMX");
    _scale.setStyle(scale == ValueScale::Percent ? kBtnSelect : kBtnIdle);
    _scale.setRegion(keep);
    present(_scale);
}

void GraphView::applyYBand(const int8_t delta) noexcept
{
    if (_tester == nullptr)
        return;
    const YBand before = _tester->yBand();
    _tester->stepYBand(delta);
    if (_tester->yBand() == before)
        return;
    syncYBandLabel();
    _plot.redrawAll();
    presentSpan();
}

void GraphView::applyScale() noexcept
{
    if (_tester == nullptr)
        return;
    _tester->toggleScale();
    syncScaleLabel();
    syncYBandLabel();
    syncChrome();
    presentDirtyLabels();
    _plot.redrawAll();
    presentSpan();
}

void GraphView::styleSpan(nex::ovl::Button& btn, const bool on) noexcept
{
    const nex::Region keep = btn.region();
    btn.setStyle(on ? kBtnIdle : kBtnDisabled);
    btn.setRegion(keep);
}

void GraphView::syncSpanLabel() noexcept
{
    const uint8_t span = (_tester != nullptr) ? _tester->traceSpanS() : 20u;
    char next[sizeof(_spanText)]{};
    std::snprintf(next, sizeof(next), "%us", static_cast<unsigned>(span));
    if (std::strcmp(_spanText, next) != 0) {
        std::memcpy(_spanText, next, sizeof(next));
        _spanLabel.setText(_spanText);
    }

    const bool canLo = span > kTraceSpanMinS;
    const bool canHi = span < kTraceSpanMaxS;
    if (canLo != _spanPrevOn) {
        _spanPrevOn = canLo;
        styleSpan(_spanPrev, canLo);
        present(_spanPrev);
    }
    if (canHi != _spanNextOn) {
        _spanNextOn = canHi;
        styleSpan(_spanNext, canHi);
        present(_spanNext);
    }
}

void GraphView::applySpan(const int8_t delta) noexcept
{
    if (_tester == nullptr)
        return;
    const uint8_t before = _tester->traceSpanS();
    _tester->stepTraceSpan(delta);
    if (_tester->traceSpanS() == before)
        return;
    syncSpanLabel();
    _plot.redrawAll();
    presentSpan();
}

void GraphView::drawBackground(const nex::AppCanvas& cs) const
{
    cs.rect_fill(screenRegion(), kBg);
    cs.rect_fill(nex::Region(nex::Point{0, 0}, nex::Rect{layout::kScreenW, layout::kTopH}), kChrome);
    cs.rect_fill(nex::Region(nex::Point{0, layout::graphFootY()}, nex::Rect{layout::kScreenW, layout::kGraphFootH}),
        kChrome);
    cs.rect_fill(nex::Region(nex::Point{0, layout::graphFootY()}, nex::Rect{layout::kScreenW, layout::kGraphRuleH}),
        kBorder);
}

void GraphView::drawBackgroundRegion(const nex::AppCanvas& cs, const nex::Region clip) const
{
    const nex::Color bg =
        (clip.ul.y < layout::kTopH || clip.ul.y >= layout::graphFootY()) ? kChrome : kBg;
    cs.rect_fill(clip, bg);
}

bool GraphView::serviceTouch() noexcept
{
    if (_overlay == nullptr)
        return false;
    _overlay->app.update();
    return isVisible() && !_overlay->isModal();
}

bool GraphView::present(nex::ovl::Object& obj) noexcept
{
    if (_overlay == nullptr || !isVisible() || _overlay->isModal())
        return false;
    redrawObject(obj, _overlay->app.cs);
    return serviceTouch();
}

void GraphView::presentSpan() noexcept
{
    if (!present(_yBandPrev) || !present(_yBandLabel) || !present(_yBandNext))
        return;
    if (!present(_spanPrev) || !present(_spanLabel))
        return;
    (void)present(_spanNext);
}

void GraphView::presentDirtyLabels() noexcept
{
    if (_link.dirty) {
        if (!present(_link))
            return;
        _link.dirty = false;
    }
    if (_pip.dirty) {
        if (!present(_pip))
            return;
        _pip.dirty = false;
    }
    if (_spanLabel.dirty) {
        if (!present(_spanLabel))
            return;
        _spanLabel.dirty = false;
    }
    if (_yBandLabel.dirty) {
        if (!present(_yBandLabel))
            return;
        _yBandLabel.dirty = false;
    }
}

void GraphView::onClick(nex::ovl::Object* const target) noexcept
{
    if (target == &_link || target == &_pip) {
        if (_tester != nullptr)
            _tester->port().clearErrors();
        syncChrome();
        presentDirtyLabels();
        return;
    }
    if (target == &_back) {
        goMonitor();
        return;
    }
    if (target == &_yBandPrev) {
        if (!_yBandPrevOn)
            return;
        applyYBand(-1);
        present(_yBandPrev);
        return;
    }
    if (target == &_yBandNext) {
        if (!_yBandNextOn)
            return;
        applyYBand(1);
        present(_yBandNext);
        return;
    }
    if (target == &_scale) {
        applyScale();
        return;
    }
    if (target == &_spanPrev) {
        if (!_spanPrevOn)
            return;
        applySpan(-1);
        present(_spanPrev);
        return;
    }
    if (target == &_spanNext) {
        if (!_spanNextOn)
            return;
        applySpan(1);
        present(_spanNext);
        return;
    }
}

void GraphView::goMonitor() noexcept
{
    hideFrom(_app.overlay);
    _app.showMonitor();
}

void GraphView::Label::setText(const char* const src) noexcept
{
    char next[sizeof(text)]{};
    copyText(next, sizeof(next), src);
    if (std::strcmp(text, next) == 0)
        return;
    std::memcpy(text, next, sizeof(text));
    dirty = true;
}

void GraphView::Label::setFg(const nex::Color color) noexcept
{
    if (fg.raw == color.raw)
        return;
    fg = color;
    dirty = true;
}

void GraphView::Label::draw(const nex::AppCanvas& cs) const
{
    if (!isVisible() || text[0] == '\0')
        return;
    cs.text_in_region(screenRegion(), 4u, text, kFont, fg, align, nex::VAlign::Center, bg, nex::BG::Color);
}

bool GraphView::Label::onTouchXY(const nex::msg::evTouchXY& e) noexcept
{
    (void)e;
    return true;
}

void GraphView::Pip::setFill(const nex::Color color) noexcept
{
    if (fill.raw == color.raw)
        return;
    fill = color;
    dirty = true;
}

void GraphView::Pip::draw(const nex::AppCanvas& cs) const
{
    if (!isVisible())
        return;
    const nex::Region r = screenRegion();
    const nex::Coord d = (r.size.w < r.size.h) ? r.size.w : r.size.h;
    if (d < 6)
        return;
    const uint16_t rad = 5u;
    const nex::Point c{
        static_cast<nex::Coord>(r.ul.x + r.size.w / 2),
        static_cast<nex::Coord>(r.ul.y + r.size.h / 2),
    };
    cs.circle_filled(c, rad, fill);
}

bool GraphView::Pip::onTouchXY(const nex::msg::evTouchXY& e) noexcept
{
    (void)e;
    return true;
}

nex::Region GraphView::Plot::plotArea() const noexcept
{
    return plotRect(screenRegion());
}

nex::Point GraphView::Plot::samplePoint(const uint8_t series, const uint16_t i) const noexcept
{
    const nex::Region plot = plotArea();
    const uint8_t v = (host != nullptr && host->_tester != nullptr) ? host->_tester->traceAt(series, i) : 0u;
    const uint8_t lo = (host != nullptr && host->_tester != nullptr) ? host->_tester->yLo() : 0u;
    const uint8_t hi = (host != nullptr && host->_tester != nullptr) ? host->_tester->yHi() : 255u;
    return nex::Point{xAtSample(plot, i), yAtValue(plot, v, lo, hi)};
}

void GraphView::Plot::drawAxes(const nex::AppCanvas& cs) const
{
    const nex::Region g = screenRegion();
    const nex::Region plot = plotRect(g);
    cs.rect_fill(g, kBg);
    drawGrid(cs);
    cs.rect_outline(plot, kTraceGrid);
}

void GraphView::Plot::drawGrid(const nex::AppCanvas& cs) const
{
    const nex::Region g = screenRegion();
    const nex::Region plot = plotRect(g);
    if (plot.size.w <= 1 || plot.size.h <= 1)
        return;

    const nex::Coord x0 = plot.ul.x;
    const nex::Coord x1 = static_cast<nex::Coord>(plot.ul.x + plot.size.w - 1);
    const nex::Coord y0 = plot.ul.y;
    const nex::Coord y1 = static_cast<nex::Coord>(plot.ul.y + plot.size.h - 1);

    const uint8_t lo = (host != nullptr && host->_tester != nullptr) ? host->_tester->yLo() : 0u;
    const uint8_t hi = (host != nullptr && host->_tester != nullptr) ? host->_tester->yHi() : 255u;
    for (uint8_t i = 0; i <= kGridY; ++i) {
        const uint8_t raw = (i >= kGridY)
            ? hi
            : static_cast<uint8_t>(lo + static_cast<uint16_t>(hi - lo) * i / kGridY);
        const nex::Coord y = gridAt(y0, plot.size.h, static_cast<uint8_t>(kGridY - i), kGridY);
        cs.line(nex::Point{x0, y}, nex::Point{x1, y}, kTraceGrid);
        if (host != nullptr && !host->serviceTouch())
            return;
        if (host != nullptr && host->_tester != nullptr)
            host->_tester->formatValue(raw, _valLbl[i], sizeof(_valLbl[i]));
        else
            std::snprintf(_valLbl[i], sizeof(_valLbl[i]), "%u", static_cast<unsigned>(raw));
        nex::VAlign valign = nex::VAlign::Center;
        nex::Coord ly = static_cast<nex::Coord>(y - 12);
        nex::Coord lh = 24;
        if (i == kGridY) {
            ly = static_cast<nex::Coord>(y - 4);
            valign = nex::VAlign::Top;
        } else if (i == 0u) {
            ly = static_cast<nex::Coord>(y - 23);
            valign = nex::VAlign::Bottom;
        }
        cs.text_in_region(nex::Region(nex::Point{g.ul.x, ly}, nex::Rect{static_cast<nex::Coord>(kAxisW - 6), lh}),
            _valLbl[i], kFont, kText, nex::HAlign::Right, valign, kBg, nex::BG::Color);
    }

    const uint16_t spanS = (host != nullptr && host->_tester != nullptr)
        ? host->_tester->traceSpanS()
        : 20u;
    uint8_t li = 0;
    for (uint8_t i = 0; i <= kGridX; ++i) {
        const nex::Coord x = gridAt(x0, plot.size.w, i, kGridX);
        cs.line(nex::Point{x, y0}, nex::Point{x, y1}, kTraceGrid);
        if (host != nullptr && !host->serviceTouch())
            return;
        const uint16_t s = static_cast<uint16_t>(static_cast<uint32_t>(i) * spanS / kGridX);
        if ((i % kTimeLabelEvery) != 0u && i != kGridX)
            continue;
        if (li >= 11u)
            continue;
        std::snprintf(_timeLbl[li], sizeof(_timeLbl[li]), "%us", static_cast<unsigned>(s));
        cs.text_in_region(
            nex::Region(nex::Point{static_cast<nex::Coord>(x - 24), static_cast<nex::Coord>(y1 + 1)},
                nex::Rect{48, static_cast<nex::Coord>(kTimeH - 1)}),
            _timeLbl[li], kFont, kText, nex::HAlign::Center, nex::VAlign::Top, kBg, nex::BG::Color);
        ++li;
    }
}

void GraphView::Plot::drawLegend(const nex::AppCanvas& cs) const
{
    if (host == nullptr || host->_tester == nullptr)
        return;
    const uint8_t n = host->_tester->traceCount();
    for (uint8_t i = 0; i < n; ++i) {
        const uint8_t col = static_cast<uint8_t>(i % kLegCols);
        const uint8_t row = static_cast<uint8_t>(i / kLegCols);
        const nex::Coord x = static_cast<nex::Coord>(kLegX0 + static_cast<int32_t>(col) * kLegItemW);
        const nex::Coord y = static_cast<nex::Coord>(static_cast<int32_t>(row) * kLegRowH);
        std::snprintf(_leg[i], sizeof(_leg[i]), "%u", static_cast<unsigned>(host->_tester->selectedAt(i)));
        cs.rect_fill(nex::Region(nex::Point{x, static_cast<nex::Coord>(y + 7)}, nex::Rect{8, 8}), kTrace[i]);
        const nex::Coord tw = static_cast<nex::Coord>(kLegItemW - 11);
        cs.text_in_region(
            nex::Region(nex::Point{static_cast<nex::Coord>(x + 11), y}, nex::Rect{tw, kLegRowH}),
            _leg[i], kFont, kTrace[i], nex::HAlign::Left, nex::VAlign::Center, kBg, nex::BG::Color);
    }
}

void GraphView::Plot::drawSeries(const nex::AppCanvas& cs, const uint16_t from, const uint16_t to) const
{
    if (host == nullptr || host->_tester == nullptr || to <= from)
        return;
    const uint8_t n = host->_tester->traceCount();
    for (uint8_t s = 0; s < n; ++s) {
        for (uint16_t i = from; i < to; ++i) {
            if (i == 0u)
                continue;
            cs.line(samplePoint(s, static_cast<uint16_t>(i - 1u)), samplePoint(s, i), kTrace[s]);
            if (host != nullptr && !host->serviceTouch())
                return;
        }
    }
}

void GraphView::Plot::draw(const nex::AppCanvas& cs) const
{
    if (!isVisible())
        return;
    drawAxes(cs);
    drawLegend(cs);
    _gen = (host != nullptr && host->_tester != nullptr) ? host->_tester->traceGen() : 0u;
    _drawn = 0;
}

void GraphView::Plot::redrawAll() noexcept
{
    _gen = 0;
    _drawn = 0;
    presentNew();
}

void GraphView::Plot::presentNew() noexcept
{
    if (host == nullptr || host->_overlay == nullptr || !isVisible() || host->_overlay->isModal()
        || host->_tester == nullptr)
        return;
    const nex::AppCanvas& cs = host->_overlay->app.cs;
    const uint16_t gen = host->_tester->traceGen();
    const uint16_t n = host->_tester->traceLen();
    if (gen != _gen || n < _drawn) {
        draw(cs);
        host->presentSpan();
        if (n > 0u) {
            drawSeries(cs, 0, n);
            _drawn = n;
        }
        return;
    }
    if (n > _drawn) {
        constexpr uint16_t kSegBudget = 4u;
        uint16_t to = n;
        if (static_cast<uint16_t>(to - _drawn) > kSegBudget)
            to = static_cast<uint16_t>(_drawn + kSegBudget);
        drawSeries(cs, _drawn, to);
        _drawn = to;
    }
}

} // namespace ui
