#include "UI/monitorView.hpp"

#include "UI/application.hpp"
#include "UI/enc.hpp"
#include "board.hpp"
#include "model/tester.hpp"

#include <cstdio>
#include <cstring>

namespace ui {

namespace {

constexpr nex::FontId kFont = 0u;

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
    , _graph{"график", nex::Rect{layout::kActionW, layout::kBtnH}, kBtnIdle}
    , _clear{"Сброс", nex::Rect{layout::kClearW, layout::kBtnH}, kBtnDisabled}
    , _pagePrev{"<", nex::Rect{layout::kNavW, layout::kBtnH}, kBtnIdle}
    , _pageNext{">", nex::Rect{layout::kNavW, layout::kBtnH}, kBtnIdle}
    , _view{"быстрые", nex::Rect{layout::kViewW, layout::kBtnH}, kBtnIdle}
    , _scale{"DMX", nex::Rect{layout::kScaleW, layout::kBtnH}, kBtnIdle}
{
    setRegion(nex::Region(nex::Point{0, 0}, nex::Rect{layout::kScreenW, layout::kScreenH}));

    _link.align = nex::HAlign::Left;
    _chVal.align = nex::HAlign::Left;
    _pageRange.align = nex::HAlign::Center;
    _link.fg = kOk;
    _chVal.fg = kTx;

    for (uint8_t row = 0; row < kMaxRows; ++row) {
        for (uint8_t col = 0; col < kCols; ++col) {
            _cells[row][col].host = this;
            _cells[row][col].col = col;
            _cells[row][col].row = row;
        }
    }

    addChrome();
    layout();
}

void MonitorView::addChrome() noexcept
{
    for (uint8_t col = 0; col < kCols; ++col)
        addChildTop(_colH[col]);
    for (uint8_t row = 0; row < kMaxRows; ++row)
        addChildTop(_rowH[row]);
    for (uint8_t row = 0; row < kMaxRows; ++row) {
        for (uint8_t col = 0; col < kCols; ++col)
            addChildTop(_cells[row][col]);
    }
    addChildTop(_pageBox);
    addChildTop(_selBox);
    addChildTop(_graph);
    addChildTop(_clear);
    addChildTop(_pagePrev);
    addChildTop(_pageRange);
    addChildTop(_pageNext);
    addChildTop(_view);
    addChildTop(_scale);
    addChildTop(_pip);
    addChildTop(_link);
    addChildTop(_chVal);
}

void MonitorView::applyOemCaptions() noexcept
{
    if (_oemReady)
        return;
    enc::utf8ToOem(_oemFast, sizeof(_oemFast), "быстрые");
    enc::utf8ToOem(_oemGraph, sizeof(_oemGraph), "график");
    enc::utf8ToOem(_oemClear, sizeof(_oemClear), "Сброс");
    RadioGroup::setLabel(_view, _oemFast);
    RadioGroup::setLabel(_graph, _oemGraph);
    RadioGroup::setLabel(_clear, _oemClear);
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
    syncHeaders();
    for (uint8_t row = 0; row < rows(); ++row) {
        for (uint8_t col = 0; col < kCols; ++col)
            (void)_cells[row][col].sync();
    }
    show(ovl);
    (void)ovl.app.pumpUntilIdle();
    _repaintChrome = false;
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
        _pip.dirty = true;
        _chVal.dirty = true;
        _repaintChrome = false;
    }
    presentDirtyCells();
    presentDirtyLabels();
}

void MonitorView::layout() noexcept
{
    layoutGrid();
    Widget::layout();
    layoutChrome();
}

void MonitorView::layoutGrid() noexcept
{
    const nex::Coord cw = layout::cellW();
    const nex::Coord ch = layout::cellH();
    const uint8_t nRows = rows();

    const nex::Coord gx = layout::gridOriginX();
    const nex::Coord gy = layout::gridOriginY();

    for (uint8_t col = 0; col < kCols; ++col) {
        _colH[col].setRegion(nex::Region(
            nex::Point{static_cast<nex::Coord>(gx + col * cw), layout::kTopH},
            nex::Rect{cw, layout::kColHdrH}));
    }
    for (uint8_t row = 0; row < kMaxRows; ++row) {
        _rowH[row].setRegion(nex::Region(
            nex::Point{0, static_cast<nex::Coord>(gy + row * ch)},
            nex::Rect{layout::kRowHdrW, ch}));
        _rowH[row].setVisible(row < nRows);
        for (uint8_t col = 0; col < kCols; ++col) {
            const nex::Region tile{
                nex::Point{
                    static_cast<nex::Coord>(gx + col * cw),
                    static_cast<nex::Coord>(gy + row * ch)},
                nex::Rect{cw, ch}};
            _cells[row][col].setRegion(nex::Canvas::innerRegion(tile, layout::kCellInset));
            _cells[row][col].setVisible(row < nRows);
        }
    }
}

void MonitorView::layoutChrome() noexcept
{
    const nex::Coord kH = layout::kBtnH;
    const nex::Coord joinY = static_cast<nex::Coord>(layout::gridY() + layout::gridFillH());
    const nex::Coord boxH = static_cast<nex::Coord>(layout::kSelPad + kH + layout::kSelPad + 2);
    const nex::Coord y1 = static_cast<nex::Coord>(joinY + layout::kSelPad);
    const nex::Coord pageBoxW = layout::pageBoxW();

    _pageBox.sides = static_cast<uint8_t>(SelFrame::Top | SelFrame::Left | SelFrame::Bottom);
    _pageBox.setVisible(true);
    _pageBox.setRegion(nex::Region(
        nex::Point{0, static_cast<nex::Coord>(joinY - 2)},
        nex::Rect{pageBoxW, static_cast<nex::Coord>(boxH + 2)}));

    nex::Coord x = static_cast<nex::Coord>(2 + layout::kSelPad);
    _pagePrev.setRegion(nex::Region(nex::Point{x, y1}, nex::Rect{layout::kNavW, kH}));
    x = static_cast<nex::Coord>(x + layout::kNavW + layout::kGap);
    _pageRange.setRegion(nex::Region(nex::Point{x, y1}, nex::Rect{layout::kPageRangeW, kH}));
    x = static_cast<nex::Coord>(x + layout::kPageRangeW + layout::kGap);
    _pageNext.setRegion(nex::Region(nex::Point{x, y1}, nex::Rect{layout::kNavW, kH}));

    const nex::Coord boxX = pageBoxW;
    const nex::Coord boxW = static_cast<nex::Coord>(layout::gridRight() - boxX);
    _selBox.sides = static_cast<uint8_t>(SelFrame::Left | SelFrame::Right | SelFrame::Bottom);
    _selBox.setVisible(true);
    _selBox.setRegion(nex::Region(nex::Point{boxX, joinY}, nex::Rect{boxW, boxH}));

    const nex::Coord innerX = static_cast<nex::Coord>(boxX + 2 + layout::kSelPad);
    _clear.setRegion(nex::Region(nex::Point{innerX, y1}, nex::Rect{layout::kClearW, kH}));
    const nex::Coord listX = static_cast<nex::Coord>(innerX + layout::kClearW + layout::kGap);
    const nex::Coord listW = static_cast<nex::Coord>(layout::gridRight() - 2 - layout::kSelPad - listX);
    _chVal.setRegion(nex::Region(nex::Point{listX, y1}, nex::Rect{listW, kH}));

    const nex::Coord y2 = layout::statusRowY();
    nex::Coord x2 = layout::kPad;
    _pip.setRegion(nex::Region(nex::Point{x2, y2}, nex::Rect{layout::kPipW, kH}));
    x2 = static_cast<nex::Coord>(x2 + layout::kPipW + layout::kGap);
    _link.setRegion(nex::Region(nex::Point{x2, y2}, nex::Rect{layout::kLinkW, kH}));
    _scale.setRegion(nex::Region(nex::Point{layout::scaleX(), y2}, nex::Rect{layout::kScaleW, kH}));
    _view.setRegion(nex::Region(nex::Point{layout::viewX(), y2}, nex::Rect{layout::kViewW, kH}));
    _graph.setRegion(nex::Region(nex::Point{layout::actionX(), y2}, nex::Rect{layout::kActionW, kH}));
}

void MonitorView::syncChrome() noexcept
{
    const uint8_t nRows = rows();
    LinkSnap snap{};
    if (_tester != nullptr) {
        _tester->setRows(nRows);
        _tester->pollLink(boardClockMs());
        snap = _tester->linkSnap();
    }

    char buf[36]{};
    enc::utf8ToOem(buf, sizeof(buf), linkCaptionUtf8(snap.state));
    _link.setText(buf);
    _link.setFg(snap.state == LinkUi::Live ? kOk : kErr);
    _pip.setFill(snap.state == LinkUi::Error ? kErr : (snap.pipOn ? kMain : kBorder));
    if (_tester != nullptr)
        _tester->formatSelection(buf, sizeof(buf));
    else
        buf[0] = '\0';
    _chVal.setText(buf);

    const bool lit = _tester != nullptr && _tester->selectedCount() > 0u;
    if (lit != _clearLit) {
        _clearLit = lit;
        styleClear(lit);
        present(_clear);
    }

    syncViewLabel();
    syncScaleLabel();
    syncPageLabel();
    syncHeaders();
}

void MonitorView::syncHeaders() noexcept
{
    const uint8_t nRows = rows();
    const uint8_t page = (_tester != nullptr) ? _tester->page() : 0u;
    const bool pageChanged = (page != _hdrPage);
    _hdrPage = page;

    char buf[5]{};
    for (uint8_t col = 0; col < kCols; ++col) {
        std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(col + 1u));
        _colH[col].setText(buf);
        if (_colH[col].dirty) {
            if (!present(_colH[col]))
                return;
            _colH[col].dirty = false;
        }
    }
    for (uint8_t row = 0; row < nRows; ++row) {
        const uint16_t chn = channelOf(page, nRows, 0u, row);
        if (chn == 0u)
            buf[0] = '\0';
        else
            std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(chn));
        _rowH[row].setText(buf);
        if (pageChanged || _rowH[row].dirty) {
            if (!present(_rowH[row]))
                return;
            _rowH[row].dirty = false;
        }
    }
}

void MonitorView::drawBackground(const nex::AppCanvas& cs) const
{
    cs.rect_fill(screenRegion(), kBg);
    cs.rect_fill(nex::Region(nex::Point{0, 0}, nex::Rect{layout::kScreenW, layout::kTopH}), kChrome);
    cs.rect_fill(nex::Region(
        nex::Point{0, layout::kTopH},
        nex::Rect{layout::kRowHdrW, layout::gridOriginY()}), kChrome);
    cs.rect_fill(nex::Region(nex::Point{0, layout::footY()}, nex::Rect{layout::kScreenW, layout::kFootH}), kChrome);
    cs.rect_fill(nex::Region(
        nex::Point{layout::kRowHdrW, layout::gridY()},
        nex::Rect{layout::gridFillW(), layout::gridFillH()}),
        kCellGrid);
    cs.rect_fill(nex::Region(
        nex::Point{0, static_cast<nex::Coord>(layout::gridY() + layout::gridFillH() - 2)},
        nex::Rect{layout::kRowHdrW, 2}),
        kCellGrid);
}

void MonitorView::drawBackgroundRegion(const nex::AppCanvas& cs, const nex::Region clip) const
{
    const nex::Color bg =
        (clip.ul.y < layout::kTopH + layout::kColHdrH || clip.ul.x < layout::kRowHdrW
            || clip.ul.y >= layout::footY())
        ? kChrome
        : kCellBg;
    cs.rect_fill(clip, bg);
}

bool MonitorView::isGridCell(const nex::ovl::Object& obj) const noexcept
{
    const auto* const first = &_cells[0][0];
    const auto* const last = &_cells[kMaxRows - 1u][kCols - 1u];
    return &obj >= first && &obj <= last;
}

bool MonitorView::enqueueDraw(nex::ovl::Object& obj) noexcept
{
    if (_overlay == nullptr || !isVisible() || _overlay->isModal())
        return false;
    const nex::AppCanvas& cs = _overlay->app.cs;
    // Клетка сама заливает фон в xstr — без чёрного fill под selected/fast.
    if (!isGridCell(obj))
        drawBackgroundRegion(cs, obj.screenRegion());
    obj.draw(cs);
    return true;
}

bool MonitorView::present(nex::ovl::Object& obj) noexcept
{
    if (!enqueueDraw(obj))
        return false;
    _overlay->app.update();
    return isVisible() && !_overlay->isModal();
}

void MonitorView::presentDirtyLabels() noexcept
{
    if (_link.dirty) {
        if (!enqueueDraw(_link))
            return;
        _link.dirty = false;
    }
    if (_pip.dirty) {
        if (!enqueueDraw(_pip))
            return;
        _pip.dirty = false;
    }
    if (_chVal.dirty) {
        if (!enqueueDraw(_chVal))
            return;
        _chVal.dirty = false;
    }
}

void MonitorView::presentDirtyCells() noexcept
{
    const uint8_t nRows = rows();
    for (uint8_t row = 0; row < nRows; ++row) {
        for (uint8_t col = 0; col < kCols; ++col) {
            if (!_cells[row][col].sync())
                continue;
            if (!enqueueDraw(_cells[row][col]))
                return;
        }
    }
}

void MonitorView::presentButtons() noexcept
{
    if (!present(_pageBox) || !present(_selBox) || !present(_graph) || !present(_clear))
        return;
    if (!present(_pagePrev) || !present(_pageRange) || !present(_pageNext))
        return;
    (void)present(_scale);
    (void)present(_view);
}

void MonitorView::styleClear(const bool lit) noexcept
{
    const nex::Region keep = _clear.region();
    _clear.setStyle(lit ? kBtnSelect : kBtnDisabled);
    _clear.setRegion(keep);
}

void MonitorView::syncViewLabel() noexcept
{
    const ViewMode view = (_tester != nullptr) ? _tester->view() : ViewMode::Current;
    if (_oemReady && view == _viewShown)
        return;
    _viewShown = view;
    const nex::Region keep = _view.region();
    _view.setStyle(view == ViewMode::Fast ? kBtnFast : kBtnIdle);
    _view.setRegion(keep);
    present(_view);
}

void MonitorView::syncScaleLabel() noexcept
{
    const ValueScale scale = (_tester != nullptr) ? _tester->scale() : ValueScale::Dmx;
    if (_oemReady && scale == _scaleShown)
        return;
    _scaleShown = scale;
    const nex::Region keep = _scale.region();
    _scale.setLabel(scale == ValueScale::Percent ? "%" : "DMX");
    _scale.setStyle(scale == ValueScale::Percent ? kBtnSelect : kBtnIdle);
    _scale.setRegion(keep);
    present(_scale);
}

void MonitorView::applyScale() noexcept
{
    if (_tester == nullptr)
        return;
    _tester->toggleScale();
    syncScaleLabel();
    syncChrome();
    presentDirtyLabels();
    const uint8_t nRows = rows();
    for (uint8_t row = 0; row < nRows; ++row) {
        for (uint8_t col = 0; col < kCols; ++col) {
            if (_cells[row][col].sync() && !present(_cells[row][col]))
                return;
        }
    }
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
        _pageRange.setText(_pageLabel);
    }
    if (_pageRange.dirty) {
        present(_pageRange);
        _pageRange.dirty = false;
    }
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
    _hdrPage = 0xFFu;
    syncHeaders();
    const uint8_t nRows = rows();
    for (uint8_t row = 0; row < nRows; ++row) {
        for (uint8_t col = 0; col < kCols; ++col) {
            if (_cells[row][col].sync() && !present(_cells[row][col]))
                return;
        }
    }
}

void MonitorView::onClick(nex::ovl::Object* const target) noexcept
{
    for (uint8_t row = 0; row < rows(); ++row) {
        for (uint8_t col = 0; col < kCols; ++col) {
            if (target == &_cells[row][col]) {
                onCellClick(_cells[row][col]);
                return;
            }
        }
    }
    if (target == &_graph) {
        _app.showGraph();
        return;
    }
    if (target == &_link || target == &_pip) {
        if (_tester != nullptr)
            _tester->port().clearErrors();
        syncChrome();
        presentDirtyLabels();
        return;
    }
    if (_tester == nullptr)
        return;

    if (target == &_clear) {
        if (_tester->selectedCount() == 0u)
            return;
        _tester->clearSelection();
        syncChrome();
        presentDirtyCells();
        presentDirtyLabels();
        return;
    }

    if (target == &_scale) {
        applyScale();
        return;
    }

    if (target == &_view) {
        const ViewMode next = (_tester->view() == ViewMode::Current) ? ViewMode::Fast : ViewMode::Current;
        _tester->setView(next);
        syncViewLabel();
        presentDirtyCells();
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

void MonitorView::onCellClick(Cell& cell) noexcept
{
    if (_tester == nullptr || cell.snap.empty())
        return;
    const uint16_t ch = channelOf(_tester->page(), rows(), cell.col, cell.row);
    if (ch == 0u)
        return;
    if (_tester->toggleSelect(ch) == Tester::SelectResult::Full)
        _app.alert("Не больше 8 каналов");
    (void)cell.sync();
    present(cell);
    syncChrome();
    presentDirtyLabels();
}

MonitorView::CellSnap MonitorView::snapOf(const uint8_t col, const uint8_t row) const noexcept
{
    if (_tester == nullptr)
        return CellSnap::vacant();

    const uint8_t nRows = rows();
    const uint16_t chan = channelOf(_tester->page(), nRows, col, row);
    if (chan == 0u)
        return CellSnap::vacant();

    return CellSnap::make(_tester->cellValue(col, row), _tester->isSelected(chan), false);
}

bool MonitorView::Cell::sync() noexcept
{
    if (host == nullptr)
        return false;
    CellSnap next = host->snapOf(col, row);
    if (!next.empty() && !snap.empty() && host->_tester != nullptr
        && host->_tester->view() == ViewMode::Fast && next.value != snap.value) {
        next = CellSnap::make(next.value, next.selected(), true);
    }
    char nextTxt[4]{};
    if (!next.empty()) {
        if (host->_tester != nullptr)
            host->_tester->formatValue(next.value, nextTxt, sizeof(nextTxt));
        else
            std::snprintf(nextTxt, sizeof(nextTxt), "%u", static_cast<unsigned>(next.value));
    }
    if (next == snap && std::strcmp(txt, nextTxt) == 0)
        return false;
    snap = next;
    std::memcpy(txt, nextTxt, sizeof(txt));
    return true;
}

void MonitorView::Cell::draw(const nex::AppCanvas& cs) const
{
    if (!isVisible())
        return;

    const nex::Region r = screenRegion();
    if (snap.empty()) {
        cs.rect_fill(r, kBg);
        return;
    }

    nex::Color bg = kCellBg;
    nex::Color fg = kCellFg;
    if (snap.fast()) {
        bg = kChangedBg;
        fg = kChangedFg;
    } else if (snap.selected()) {
        bg = kSelect;
        fg = kChangedFg;
    }
    cs.text_in_region(r, txt, kFont, fg, nex::HAlign::Center, nex::VAlign::Center, bg, nex::BG::Color);
}

bool MonitorView::Cell::onTouchXY(const nex::msg::evTouchXY& e) noexcept
{
    (void)e;
    return !snap.empty();
}

void MonitorView::Head::setText(const char* const src) noexcept
{
    char next[sizeof(text)]{};
    copyText(next, sizeof(next), src);
    if (std::strcmp(text, next) == 0)
        return;
    std::memcpy(text, next, sizeof(text));
    dirty = true;
}

void MonitorView::Head::draw(const nex::AppCanvas& cs) const
{
    if (!isVisible())
        return;
    cs.text_in_region(screenRegion(), text, kFont, kText, nex::HAlign::Center, nex::VAlign::Center, kChrome,
        nex::BG::Color);
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

bool MonitorView::Label::onTouchXY(const nex::msg::evTouchXY& e) noexcept
{
    (void)e;
    return true;
}

void MonitorView::Pip::setFill(const nex::Color color) noexcept
{
    if (fill.raw == color.raw)
        return;
    fill = color;
    dirty = true;
}

void MonitorView::Pip::draw(const nex::AppCanvas& cs) const
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

bool MonitorView::Pip::onTouchXY(const nex::msg::evTouchXY& e) noexcept
{
    (void)e;
    return true;
}

void MonitorView::SelFrame::draw(const nex::AppCanvas& cs) const
{
    if (!isVisible())
        return;
    const nex::Region r = screenRegion();
    cs.rect_fill(r, kChrome);
    if ((sides & Left) != 0u)
        cs.rect_fill(nex::Region(r.ul, nex::Rect{2, r.size.h}), kBorder);
    if ((sides & Right) != 0u) {
        cs.rect_fill(nex::Region(
            nex::Point{static_cast<nex::Coord>(r.ul.x + r.size.w - 2), r.ul.y},
            nex::Rect{2, r.size.h}), kBorder);
    }
    if ((sides & Bottom) != 0u) {
        cs.rect_fill(nex::Region(
            nex::Point{r.ul.x, static_cast<nex::Coord>(r.ul.y + r.size.h - 2)},
            nex::Rect{r.size.w, 2}), kBorder);
    }
    if ((sides & Top) != 0u)
        cs.rect_fill(nex::Region(r.ul, nex::Rect{r.size.w, 2}), kBorder);
}

} // namespace ui
