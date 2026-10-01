#include "UI/monitorView.hpp"

#include "UI/application.hpp"
#include "UI/enc.hpp"
#include "model/tester.hpp"

#include <cstdio>
#include <cstring>

namespace ui {

namespace {

constexpr nex::FontId kFont = 0u;
constexpr uint8_t kDirtyCellsPerTick = 32u;

static_assert(layout::kScreenW == nex::hmi::kScreenW && layout::kScreenH == nex::hmi::kScreenH,
    "layout must match HMI 480x272");

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

MonitorView::MonitorView(Application& app) noexcept
    : _app(app)
    , _graph{"график", nex::Rect{72, layout::kBtnH}, kBtnIdle}
    , _pagePrev{"<", nex::Rect{28, layout::kBtnH}, kBtnIdle}
    , _pageNext{">", nex::Rect{28, layout::kBtnH}, kBtnIdle}
    , _pageRange{"1-32", nex::Rect{80, layout::kBtnH}, kBtnOn}
{
    setRegion(nex::Region(nex::Point{0, 0}, nex::Rect{layout::kScreenW, layout::kScreenH}));

    static constexpr const char* kViewLabel[2] = {"текущие", "быстрые"};
    for (uint8_t i = 0; i < 2u; ++i)
        _viewBtn[i] = nex::ovl::Button{kViewLabel[i], nex::Rect{96, layout::kBtnH}, i == 0u ? kBtnOn : kBtnIdle};

    _grid.host = this;
    _link.align = nex::HAlign::Left;
    _cycle.align = nex::HAlign::Left;
    _chVal.align = nex::HAlign::Left;
    _link.fg = kOk;
    _cycle.fg = kText;
    _chVal.fg = kTx;

    bindRadioGroups();
    addChrome();
    layout();
}

void MonitorView::addChrome() noexcept
{
    addChildTop(_grid);
    addChildTop(_graph);
    addChildTop(_pagePrev);
    addChildTop(_pageRange);
    addChildTop(_pageNext);
    for (uint8_t i = 0; i < 2u; ++i)
        addChildTop(_viewBtn[i]);
    addChildTop(_link);
    addChildTop(_cycle);
    addChildTop(_chVal);
}

void MonitorView::applyOemCaptions() noexcept
{
    if (_oemReady)
        return;
    enc::utf8ToOem(_oemCurrent, sizeof(_oemCurrent), "текущие");
    enc::utf8ToOem(_oemFast, sizeof(_oemFast), "быстрые");
    enc::utf8ToOem(_oemGraph, sizeof(_oemGraph), "график");
    RadioGroup::setLabel(_viewBtn[0], _oemCurrent);
    RadioGroup::setLabel(_viewBtn[1], _oemFast);
    RadioGroup::setLabel(_graph, _oemGraph);
    _oemReady = true;
}

void MonitorView::showOn(nex::ovl::Overlay& ovl) noexcept
{
    applyOemCaptions();
    _overlay = &ovl;
    if (_tester != nullptr)
        _tester->setRows(rows());
    layout();
    syncChrome();
    show(ovl);
    _repaintChrome = true;
    _grid.presentDirty();
    presentDirtyLabels();
}

void MonitorView::hideFrom(nex::ovl::Overlay& ovl) noexcept
{
    hide(ovl);
    if (_overlay == &ovl)
        _overlay = nullptr;
}

void MonitorView::refresh() noexcept
{
    if (_overlay == nullptr || !isVisible())
        return;
    syncChrome();
    if (_repaintChrome) {
        presentButtons();
        _link.dirty = true;
        _cycle.dirty = true;
        _chVal.dirty = true;
        _repaintChrome = false;
    }
    _grid.presentDirty();
    presentDirtyLabels();
}

void MonitorView::layout() noexcept
{
    const uint8_t nRows = rows();
    const nex::Coord ch = layout::cellH();

    _grid.setRegion(nex::Region(
        nex::Point{0, layout::kTopH},
        nex::Rect{layout::kScreenW, static_cast<nex::Coord>(layout::kColHdrH + ch * nRows)}));

    Widget::layout();
    layoutChrome();
}

void MonitorView::layoutChrome() noexcept
{
    const nex::Coord kH = layout::kBtnH;
    const nex::Coord fy = layout::footY();
    const nex::Coord y1 = static_cast<nex::Coord>(fy + 2);
    nex::Coord x = layout::kPad;
    _pagePrev.setRegion(nex::Region(nex::Point{x, y1}, nex::Rect{28, kH}));
    x = static_cast<nex::Coord>(x + 28 + layout::kGap);
    _pageRange.setRegion(nex::Region(nex::Point{x, y1}, nex::Rect{80, kH}));
    x = static_cast<nex::Coord>(x + 80 + layout::kGap);
    _pageNext.setRegion(nex::Region(nex::Point{x, y1}, nex::Rect{28, kH}));

    x = static_cast<nex::Coord>(layout::kScreenW - layout::kPad);
    for (int i = 1; i >= 0; --i) {
        x = static_cast<nex::Coord>(x - 96);
        _viewBtn[static_cast<uint8_t>(i)].setRegion(nex::Region(nex::Point{x, y1}, nex::Rect{92, kH}));
        x = static_cast<nex::Coord>(x - layout::kGap);
    }

    const nex::Coord y2 = static_cast<nex::Coord>(fy + 38);
    _link.setRegion(nex::Region(nex::Point{layout::kPad, y2}, nex::Rect{72, kH}));
    _cycle.setRegion(nex::Region(nex::Point{80, y2}, nex::Rect{64, kH}));
    _chVal.setRegion(nex::Region(nex::Point{148, y2}, nex::Rect{248, kH}));
    _graph.setRegion(nex::Region(nex::Point{400, y2}, nex::Rect{72, kH}));
}

void MonitorView::syncChrome() noexcept
{
    const uint8_t nRows = rows();
    ViewMode view = ViewMode::Current;
    uint32_t cycle = 0;
    const char* link = "-";
    if (_tester != nullptr) {
        _tester->setRows(nRows);
        view = _tester->view();
        cycle = _tester->port().frameCount();
        link = statusText(_tester->port().getStatus());
    }

    _views.sync(static_cast<uint8_t>(view));

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

    syncPageLabel();
}

void MonitorView::drawBackground(const nex::AppCanvas& cs) const
{
    cs.rect_fill(screenRegion(), kBg);
    cs.rect_fill(nex::Region(nex::Point{0, 0}, nex::Rect{layout::kScreenW, layout::kTopH}), kChrome);
    cs.rect_fill(nex::Region(nex::Point{0, layout::footY()}, nex::Rect{layout::kScreenW, layout::kFootH}), kChrome);
}

void MonitorView::drawBackgroundRegion(const nex::AppCanvas& cs, const nex::Region clip) const
{
    const nex::Color bg =
        (clip.ul.y < layout::kTopH || clip.ul.y >= layout::footY()) ? kChrome : kBg;
    cs.rect_fill(clip, bg);
}

void MonitorView::present(nex::ovl::Object& obj) noexcept
{
    if (_overlay == nullptr || !isVisible() || _overlay->isModal())
        return;
    redrawObject(obj, _overlay->app.cs);
}

void MonitorView::presentDirtyLabels() noexcept
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

void MonitorView::presentButtons() noexcept
{
    present(_graph);
    present(_pagePrev);
    present(_pageRange);
    present(_pageNext);
    for (uint8_t i = 0; i < 2u; ++i)
        present(_viewBtn[i]);
}

void MonitorView::presentRadio(const RadioGroup::Paint& p) noexcept
{
    if (p.a != nullptr)
        present(*p.a);
    if (p.b != nullptr && p.b != p.a)
        present(*p.b);
}

void MonitorView::presentSelectedLabel() noexcept
{
    if (_tester == nullptr)
        return;
    char buf[36]{};
    _tester->formatSelection(buf, sizeof(buf));
    _chVal.setText(buf);
    presentDirtyLabels();
}

void MonitorView::bindRadioGroups() noexcept
{
    for (uint8_t i = 0; i < 2u; ++i)
        _views.bind(_viewBtn[i]);
}

void MonitorView::syncPageLabel() noexcept
{
    const uint8_t nRows = rows();
    const uint8_t page = (_tester != nullptr) ? _tester->page() : 0u;
    char next[sizeof(_pageLabel)]{};
    const uint16_t a = pageFirst(page, nRows);
    const uint16_t b = pageLast(page, nRows);
    std::snprintf(next, sizeof(next), "%u-%u", static_cast<unsigned>(a), static_cast<unsigned>(b));
    if (std::strcmp(_pageLabel, next) != 0) {
        std::memcpy(_pageLabel, next, sizeof(next));
        RadioGroup::setLabel(_pageRange, _pageLabel);
    }
    RadioGroup::style(_pageRange, true);
}

void MonitorView::applyPage(const int8_t delta) noexcept
{
    if (_tester == nullptr)
        return;
    const uint8_t before = _tester->page();
    _tester->stepPage(delta);
    if (_tester->page() == before)
        return;
    syncPageLabel();
    present(_pagePrev);
    present(_pageRange);
    present(_pageNext);
    _grid.presentDirty();
}

void MonitorView::onClick(nex::ovl::Object* const target) noexcept
{
    if (target == &_grid) {
        onGridClick();
        return;
    }
    if (target == &_graph) {
        _app.showGraph();
        return;
    }
    if (_tester == nullptr)
        return;

    const RadioGroup::Paint viewPaint = _views.select(target);
    if (viewPaint.hit) {
        _tester->setView(static_cast<ViewMode>(_views.selected()));
        presentRadio(viewPaint);
        _grid.presentDirty();
        return;
    }

    if (target == &_pagePrev) {
        applyPage(-1);
        present(_pagePrev);
        return;
    }
    if (target == &_pageNext) {
        applyPage(1);
        present(_pageNext);
        return;
    }
}

void MonitorView::onTouch(const nex::msg::evTouchXY& e) noexcept
{
    if (e.state == nex::TouchState::Press) {
        if (!_grid.screenRegion().contains(e.pos))
            _grid.haveHit = false;
        return;
    }
    if (e.state == nex::TouchState::Release && _grid.haveHit) {
        onGridClick();
        _grid.haveHit = false;
    }
}

void MonitorView::onGridClick() noexcept
{
    if (!_grid.haveHit || _tester == nullptr)
        return;
    const uint16_t ch = channelOf(_tester->page(), rows(), _grid.hitCol, _grid.hitRow);
    if (ch == 0u)
        return;
    if (_tester->toggleSelect(ch) == Tester::SelectResult::Full)
        _app.alert("Не больше 8 каналов");
    syncChrome();
    _grid.presentDirty();
    presentDirtyLabels();
}

void MonitorView::Label::setText(const char* const src) noexcept
{
    char next[sizeof(text)]{};
    copyText(next, sizeof(next), src);
    if (std::strcmp(text, next) == 0)
        return;
    std::memcpy(text, next, sizeof(text));
    dirty = true;
}

void MonitorView::Label::setFg(const nex::Color color) noexcept
{
    if (fg.raw == color.raw)
        return;
    fg = color;
    dirty = true;
}

void MonitorView::Label::draw(const nex::AppCanvas& cs) const
{
    if (!isVisible() || text[0] == '\0')
        return;
    cs.text_in_region(screenRegion(), 4u, text, kFont, fg, align, nex::VAlign::Center, bg, nex::BG::Color);
}

MonitorView::CellSnap MonitorView::Grid::snapOf(const uint8_t col, const uint8_t row) const noexcept
{
    if (host == nullptr)
        return CellSnap::vacant();

    Tester* const t = host->_tester;
    const uint8_t nRows = host->rows();
    const uint8_t page = (t != nullptr) ? t->page() : 0u;
    const uint16_t chan = channelOf(page, nRows, col, row);
    if (chan == 0u)
        return CellSnap::vacant();

    uint8_t v = 0;
    bool fast = false;
    bool sel = false;
    if (t != nullptr) {
        v = t->cellValue(col, row);
        fast = (t->view() == ViewMode::Fast) && t->cellChanged(col, row);
        sel = t->isSelected(chan);
    }
    return CellSnap::make(v, sel, fast);
}

void MonitorView::Grid::resetCache(const uint8_t page, const uint8_t nRows, const ViewMode view) const noexcept
{
    for (uint8_t row = 0; row < kMaxRows; ++row) {
        for (uint8_t col = 0; col < kCols; ++col)
            _cache[row][col] = CellSnap::vacant();
    }
    _cachePage = page;
    _cacheRows = nRows;
    _cacheView = view;
}

void MonitorView::Grid::drawHeaders(const nex::AppCanvas& cs) const
{
    const nex::Coord cw = layout::cellW();
    const nex::Region g = screenRegion();
    cs.text_in_region(nex::Region(g.ul, nex::Rect{layout::kRowHdrW, layout::kColHdrH}), "", kFont, kText,
        nex::HAlign::Center, nex::VAlign::Center, kChrome, nex::BG::Color);
    for (uint8_t col = 0; col < kCols; ++col) {
        std::snprintf(_hdr[col], sizeof(_hdr[col]), "%u", static_cast<unsigned>(col + 1u));
        const nex::Region hr(
            nex::Point{static_cast<nex::Coord>(g.ul.x + layout::kRowHdrW + col * cw), g.ul.y},
            nex::Rect{cw, layout::kColHdrH});
        cs.text_in_region(hr, _hdr[col], kFont, kText, nex::HAlign::Center, nex::VAlign::Center, kChrome,
            nex::BG::Color);
    }
    drawRowHeaders(cs);
    _headersOk = true;
}

void MonitorView::Grid::drawRowHeaders(const nex::AppCanvas& cs) const
{
    const nex::Coord ch = layout::cellH();
    const nex::Region g = screenRegion();
    const uint8_t nRows = host != nullptr ? host->rows() : 0u;
    const uint8_t page = (host != nullptr && host->_tester != nullptr) ? host->_tester->page() : 0u;
    for (uint8_t row = 0; row < nRows; ++row) {
        const uint16_t chn = channelOf(page, nRows, 0u, row);
        const nex::Coord y = static_cast<nex::Coord>(g.ul.y + layout::kColHdrH + row * ch);
        if (chn == 0u) {
            _rowHdr[row][0] = '\0';
            cs.rect_fill(nex::Region(nex::Point{g.ul.x, y}, nex::Rect{g.size.w, ch}), kBg);
            continue;
        }
        std::snprintf(_rowHdr[row], sizeof(_rowHdr[row]), "%u", static_cast<unsigned>(chn));
        const nex::Region hr(nex::Point{g.ul.x, y}, nex::Rect{layout::kRowHdrW, ch});
        cs.text_in_region(hr, _rowHdr[row], kFont, kText, nex::HAlign::Center, nex::VAlign::Center, kChrome,
            nex::BG::Color);
    }
}

void MonitorView::Grid::drawCell(const nex::AppCanvas& cs, const uint8_t col, const uint8_t row,
    const CellSnap& snap) const
{
    if (snap.empty())
        return;

    nex::Color bg = kCellBg;
    nex::Color fg = kCellFg;
    if (snap.fast()) {
        bg = kChangedBg;
        fg = kChangedFg;
    } else if (snap.selected()) {
        bg = kSelect;
        fg = kChangedFg;
    }
    std::snprintf(_txt[row][col], sizeof(_txt[row][col]), "%u", static_cast<unsigned>(snap.value));
    const nex::Region inner = nex::Canvas::innerRegion(cellRegion(col, row), 1u);
    cs.text_in_region(inner, _txt[row][col], kFont, fg, nex::HAlign::Center, nex::VAlign::Center, bg,
        nex::BG::Color);
}

void MonitorView::Grid::draw(const nex::AppCanvas& cs) const
{
    if (host == nullptr || !isVisible())
        return;

    const uint8_t nRows = host->rows();
    Tester* const t = host->_tester;
    const uint8_t page = (t != nullptr) ? t->page() : 0u;
    const ViewMode view = (t != nullptr) ? t->view() : ViewMode::Current;
    resetCache(page, nRows, view);
    const nex::Region g = screenRegion();
    cs.rect_fill(nex::Region(
        nex::Point{g.ul.x, static_cast<nex::Coord>(g.ul.y + layout::kColHdrH)},
        nex::Rect{g.size.w, static_cast<nex::Coord>(g.size.h - layout::kColHdrH)}), kCellGrid);
    drawHeaders(cs);

    for (uint8_t row = 0; row < nRows; ++row) {
        for (uint8_t col = 0; col < kCols; ++col) {
            const CellSnap snap = snapOf(col, row);
            drawCell(cs, col, row, snap);
            _cache[row][col] = snap;
        }
    }
}

void MonitorView::Grid::presentDirty() noexcept
{
    if (host == nullptr || host->_overlay == nullptr || !isVisible() || host->_overlay->isModal())
        return;

    const nex::AppCanvas& cs = host->_overlay->app.cs;
    const uint8_t nRows = host->rows();
    Tester* const t = host->_tester;
    const uint8_t page = (t != nullptr) ? t->page() : 0u;
    const ViewMode view = (t != nullptr) ? t->view() : ViewMode::Current;

    if (!_headersOk)
        drawHeaders(cs);

    if (page != _cachePage || nRows != _cacheRows || view != _cacheView) {
        resetCache(page, nRows, view);
        drawRowHeaders(cs);
        _headersOk = true;
        for (uint8_t row = 0; row < nRows; ++row) {
            for (uint8_t col = 0; col < kCols; ++col) {
                const CellSnap snap = snapOf(col, row);
                if (!snap.empty())
                    drawCell(cs, col, row, snap);
                _cache[row][col] = snap;
            }
        }
        (void)host->_overlay->app.pumpUntilIdle();
        return;
    }

    uint8_t drawn = 0;
    for (uint8_t row = 0; row < nRows && drawn < kDirtyCellsPerTick; ++row) {
        for (uint8_t col = 0; col < kCols && drawn < kDirtyCellsPerTick; ++col) {
            const CellSnap snap = snapOf(col, row);
            if (snap == _cache[row][col])
                continue;
            drawCell(cs, col, row, snap);
            _cache[row][col] = snap;
            ++drawn;
        }
    }
}

nex::Region MonitorView::Grid::cellRegion(const uint8_t col, const uint8_t row) const noexcept
{
    const nex::Region g = screenRegion();
    const nex::Coord cw = layout::cellW();
    const nex::Coord ch = layout::cellH();
    return nex::Region(
        nex::Point{
            static_cast<nex::Coord>(g.ul.x + layout::kRowHdrW + static_cast<nex::Coord>(col) * cw),
            static_cast<nex::Coord>(g.ul.y + layout::kColHdrH + static_cast<nex::Coord>(row) * ch)},
        nex::Rect{cw, ch});
}

bool MonitorView::Grid::onTouchXY(const nex::msg::evTouchXY& e) noexcept
{
    if (host == nullptr || e.state != nex::TouchState::Press)
        return false;

    haveHit = false;
    const uint8_t nRows = host->rows();
    for (uint8_t row = 0; row < nRows; ++row) {
        for (uint8_t col = 0; col < kCols; ++col) {
            if (cellRegion(col, row).contains(e.pos)) {
                hitCol = col;
                hitRow = row;
                haveHit = true;
                break;
            }
        }
        if (haveHit)
            break;
    }
    return false;
}

} // namespace ui
