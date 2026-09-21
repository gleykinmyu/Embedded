#include "UI/graphView.hpp"

#include "UI/application.hpp"
#include "UI/enc.hpp"
#include "model/tester.hpp"

#include <cstdio>
#include <cstring>

namespace ui {

namespace {

constexpr nex::FontId kFont = 0u;
constexpr nex::Coord kLegendH = 24;
constexpr nex::Coord kAxisW = 48;
constexpr nex::Coord kTimeH = 22;
constexpr uint8_t kGridX = 20u;
constexpr uint8_t kGridY = 8u;
constexpr uint16_t kTimeLabelStepS = 2u;

[[nodiscard]] nex::Coord gridStep(const nex::Coord availW, const nex::Coord availH) noexcept
{
    const int32_t sx = (availW > 1) ? (static_cast<int32_t>(availW) - 1) / kGridX : 1;
    const int32_t sy = (availH > 1) ? (static_cast<int32_t>(availH) - 1) / kGridY : 1;
    const int32_t s = (sx < sy) ? sx : sy;
    return static_cast<nex::Coord>(s > 0 ? s : 1);
}

[[nodiscard]] nex::Region plotRect(const nex::Region& g) noexcept
{
    const nex::Coord availW = static_cast<nex::Coord>(g.size.w - kAxisW - 8);
    const nex::Coord availH = static_cast<nex::Coord>(g.size.h - kLegendH - kTimeH);
    const nex::Coord s = gridStep(availW, availH);
    const nex::Coord pw = static_cast<nex::Coord>(static_cast<int32_t>(s) * kGridX + 1);
    const nex::Coord ph = static_cast<nex::Coord>(static_cast<int32_t>(s) * kGridY + 1);
    const nex::Coord x = static_cast<nex::Coord>(g.ul.x + kAxisW);
    const nex::Coord y = static_cast<nex::Coord>(g.ul.y + kLegendH + (availH - ph) / 2);
    return nex::Region(nex::Point{x, y}, nex::Rect{pw, ph});
}

[[nodiscard]] nex::Coord xAtSample(const nex::Region& plot, const uint16_t i) noexcept
{
    const uint16_t use = (kTraceLen > 1u) ? static_cast<uint16_t>(kTraceLen - 1u) : 1u;
    const uint16_t clamped = (i >= kTraceLen) ? use : i;
    return static_cast<nex::Coord>(
        static_cast<int32_t>(plot.ul.x) + static_cast<int32_t>(clamped) * (plot.size.w - 1) / use);
}

[[nodiscard]] nex::Coord yAtValue(const nex::Region& plot, const uint8_t v) noexcept
{
    return static_cast<nex::Coord>(static_cast<int32_t>(plot.ul.y + plot.size.h - 1)
        - static_cast<int32_t>(v) * (plot.size.h - 1) / 255);
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
    , _dir{"RX", nex::Rect{88, 40}, kBtnOn}
    , _back{"сетка", nex::Rect{100, 40}, kBtnIdle}
    , _zero{"0", nex::Rect{56, 40}, kBtnIdle}
    , _full{"255", nex::Rect{64, 40}, kBtnIdle}
    , _blk{"BLK", nex::Rect{64, 40}, kBtnIdle}
{
    setRegion(nex::Region(nex::Point{0, 0}, nex::Rect{layout::kScreenW, layout::kScreenH}));
    _plot.host = this;
    _link.align = nex::HAlign::Left;
    _cycle.align = nex::HAlign::Left;
    _chVal.align = nex::HAlign::Left;
    _link.fg = kOk;
    _cycle.fg = kText;
    _chVal.fg = kTx;

    addChildTop(_plot);
    addChildTop(_dir);
    addChildTop(_back);
    addChildTop(_link);
    addChildTop(_cycle);
    addChildTop(_chVal);
    addChildTop(_zero);
    addChildTop(_full);
    addChildTop(_blk);
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
    _cycle.dirty = true;
    _chVal.dirty = true;
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
    const nex::Coord fy = layout::footY();
    _plot.setRegion(nex::Region(
        nex::Point{0, layout::kTopH},
        nex::Rect{layout::kScreenW, static_cast<nex::Coord>(fy - layout::kTopH)}));
    Widget::layout();
    layoutChrome();
}

void GraphView::layoutChrome() noexcept
{
    constexpr nex::Coord kY = 4;
    constexpr nex::Coord kH = 40;
    _dir.setRegion(nex::Region(nex::Point{layout::kPad, kY}, nex::Rect{88, kH}));
    _back.setRegion(nex::Region(nex::Point{104, kY}, nex::Rect{100, kH}));

    const nex::Coord y2 = static_cast<nex::Coord>(layout::footY() + 56);
    _link.setRegion(nex::Region(nex::Point{layout::kPad, y2}, nex::Rect{160, 40}));
    _cycle.setRegion(nex::Region(nex::Point{176, y2}, nex::Rect{140, 40}));
    _chVal.setRegion(nex::Region(nex::Point{324, y2}, nex::Rect{428, 40}));
    _zero.setRegion(nex::Region(nex::Point{760, y2}, nex::Rect{56, 40}));
    _full.setRegion(nex::Region(nex::Point{824, y2}, nex::Rect{64, 40}));
    _blk.setRegion(nex::Region(nex::Point{896, y2}, nex::Rect{64, 40}));
}

void GraphView::syncChrome() noexcept
{
    dmx::Direction dir = dmx::Direction::Receive;
    uint32_t cycle = 0;
    const char* link = "-";
    if (_tester != nullptr) {
        dir = _tester->port().direction();
        cycle = _tester->port().frameCount();
        link = dmx::cstr(_tester->port().getStatus());
    }
    RadioGroup::setLabel(_dir, dir == dmx::Direction::Transmit ? "TX" : "RX");
    RadioGroup::style(_dir, dir == dmx::Direction::Transmit);

    char buf[36]{};
    _link.setText(link);
    _link.setFg((std::strcmp(link, "OK") == 0 || std::strcmp(link, "-") == 0) ? kOk : kErr);
    std::snprintf(buf, sizeof(buf), "#%lu", static_cast<unsigned long>(cycle));
    _cycle.setText(buf);
    if (_tester != nullptr)
        _tester->formatSelection(buf, sizeof(buf));
    else
        buf[0] = '\0';
    _chVal.setText(buf);
}

void GraphView::drawBackground(const nex::AppCanvas& cs) const
{
    cs.rect_fill(screenRegion(), kBg);
    cs.rect_fill(nex::Region(nex::Point{0, 0}, nex::Rect{layout::kScreenW, layout::kTopH}), kChrome);
    cs.rect_fill(nex::Region(nex::Point{0, layout::footY()}, nex::Rect{layout::kScreenW, layout::kFootH}), kChrome);
}

void GraphView::drawBackgroundRegion(const nex::AppCanvas& cs, const nex::Region clip) const
{
    const nex::Color bg =
        (clip.ul.y < layout::kTopH || clip.ul.y >= layout::footY()) ? kChrome : kBg;
    cs.rect_fill(clip, bg);
}

void GraphView::present(nex::ovl::Object& obj) noexcept
{
    if (_overlay == nullptr || !isVisible() || _overlay->isModal())
        return;
    redrawObject(obj, _overlay->app.cs);
}

void GraphView::presentDirtyLabels() noexcept
{
    if (_link.dirty) {
        present(_link);
        _link.dirty = false;
    }
    if (_cycle.dirty) {
        present(_cycle);
        _cycle.dirty = false;
    }
    if (_chVal.dirty) {
        present(_chVal);
        _chVal.dirty = false;
    }
}

void GraphView::onClick(nex::ovl::Object* const target) noexcept
{
    if (target == &_back) {
        goMonitor();
        return;
    }
    if (_tester == nullptr)
        return;
    if (target == &_dir) {
        const auto next = (_tester->port().direction() == dmx::Direction::Receive)
            ? dmx::Direction::Transmit
            : dmx::Direction::Receive;
        _tester->port().setDirection(next);
        const bool tx = next == dmx::Direction::Transmit;
        RadioGroup::setLabel(_dir, tx ? "TX" : "RX");
        RadioGroup::style(_dir, tx);
        present(_dir);
        return;
    }
    if (target == &_zero || target == &_full || target == &_blk) {
        if (target == &_zero)
            _tester->setSelectedValues(0);
        else if (target == &_full)
            _tester->setSelectedValues(255);
        else
            _tester->blackout();
        if (_tester->port().direction() == dmx::Direction::Transmit)
            _tester->sendLive();
        present(*static_cast<nex::ovl::Button*>(target));
        presentDirtyLabels();
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

nex::Region GraphView::Plot::plotArea() const noexcept
{
    return plotRect(screenRegion());
}

nex::Point GraphView::Plot::samplePoint(const uint8_t series, const uint16_t i) const noexcept
{
    const nex::Region plot = plotArea();
    const uint8_t v = (host != nullptr && host->_tester != nullptr) ? host->_tester->traceAt(series, i) : 0u;
    return nex::Point{xAtSample(plot, i), yAtValue(plot, v)};
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
    const nex::Coord step = static_cast<nex::Coord>((plot.size.h - 1) / kGridY);

    for (uint8_t i = 0; i <= kGridY; ++i) {
        const uint8_t v = (i >= kGridY) ? 255u : static_cast<uint8_t>(i * 32u);
        const nex::Coord y = static_cast<nex::Coord>(y1 - static_cast<int32_t>(i) * step);
        cs.line(nex::Point{x0, y}, nex::Point{x1, y}, kTraceGrid);
        std::snprintf(_valLbl[i], sizeof(_valLbl[i]), "%u", static_cast<unsigned>(v));
        nex::VAlign valign = nex::VAlign::Center;
        nex::Coord ly = static_cast<nex::Coord>(y - 10);
        nex::Coord lh = 20;
        if (i == kGridY) {
            ly = y;
            valign = nex::VAlign::Top;
        } else if (i == 0u) {
            ly = static_cast<nex::Coord>(y - 19);
            valign = nex::VAlign::Bottom;
        }
        cs.text_in_region(nex::Region(nex::Point{g.ul.x, ly}, nex::Rect{static_cast<nex::Coord>(kAxisW - 4), lh}),
            _valLbl[i], kFont, kText, nex::HAlign::Right, valign, kBg, nex::BG::Color);
    }

    const uint16_t spanS =
        static_cast<uint16_t>(((static_cast<uint32_t>(kTraceLen) - 1u) * kTracePeriodMs + 500u) / 1000u);
    uint8_t li = 0;
    for (uint8_t i = 0; i <= kGridX; ++i) {
        const nex::Coord x = static_cast<nex::Coord>(x0 + static_cast<int32_t>(i) * step);
        cs.line(nex::Point{x, y0}, nex::Point{x, y1}, kTraceGrid);
        const uint16_t s = static_cast<uint16_t>(static_cast<uint32_t>(i) * spanS / kGridX);
        if ((s % kTimeLabelStepS) != 0u && i != kGridX)
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
    const nex::Region g = screenRegion();
    nex::Coord x = static_cast<nex::Coord>(g.ul.x + kAxisW);
    const uint8_t n = host->_tester->traceCount();
    for (uint8_t i = 0; i < n; ++i) {
        std::snprintf(_leg[i], sizeof(_leg[i]), "%u", static_cast<unsigned>(host->_tester->selectedAt(i)));
        const nex::Region sw(nex::Point{x, static_cast<nex::Coord>(g.ul.y + 4)}, nex::Rect{16, 12});
        cs.rect_fill(sw, kTrace[i]);
        const nex::Region lb(nex::Point{static_cast<nex::Coord>(x + 20), g.ul.y}, nex::Rect{56, kLegendH});
        cs.text_in_region(lb, _leg[i], kFont, kTrace[i], nex::HAlign::Left, nex::VAlign::Center, kBg,
            nex::BG::Color);
        x = static_cast<nex::Coord>(x + 80);
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
    _drawn = (host != nullptr && host->_tester != nullptr) ? host->_tester->traceLen() : 0u;
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
