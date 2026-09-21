#include "UI/monitorView.hpp"

#include "UI/application.hpp"
#include "model/tester.hpp"

#include <cstdio>
#include <cstring>

namespace ui {

namespace {

constexpr nex::FontId kFont = 0u;

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
    , _dir{"RX", nex::Rect{88, 40}, kBtnOn}
    , _rs485{"RS485", nex::Rect{100, 40}, kBtnOn}
    , _art{"Art-Net", nex::Rect{110, 40}, kBtnIdle}
    , _sacn{"sACN", nex::Rect{88, 40}, kBtnIdle}
    , _net{"Сеть", nex::Rect{88, 40}, kBtnIdle}
    , _zero{"0", nex::Rect{56, 40}, kBtnIdle}
    , _full{"255", nex::Rect{64, 40}, kBtnIdle}
    , _blk{"BLK", nex::Rect{64, 40}, kBtnIdle}
{
    setRegion(nex::Region(nex::Point{0, 0}, nex::Rect{layout::kScreenW, layout::kScreenH}));

    static constexpr const char* kViewLabel[5] = {"текущие", "быстрый", "лог", "min", "max"};
    for (uint8_t i = 0; i < 5u; ++i)
        _viewBtn[i] = nex::ovl::Button{kViewLabel[i], nex::Rect{112, 40}, i == 0u ? kBtnOn : kBtnIdle};

    for (uint8_t i = 0; i < kMaxPages; ++i)
        _pageBtn[i] = nex::ovl::Button{_pageLabel[i], nex::Rect{120, 40}, kBtnIdle};

    _grid.host = this;
    _univ.align = nex::HAlign::Center;
    _link.align = nex::HAlign::Left;
    _cycle.align = nex::HAlign::Left;
    _chVal.align = nex::HAlign::Left;
    _univ.fg = kText;
    _link.fg = kOk;
    _cycle.fg = kText;
    _chVal.fg = kTx;

    addChrome();
    layout();
}

void MonitorView::addChrome() noexcept
{
    addChildTop(_grid);
    addChildTop(_dir);
    addChildTop(_rs485);
    addChildTop(_art);
    addChildTop(_sacn);
    addChildTop(_univ);
    addChildTop(_net);
    for (uint8_t i = 0; i < kMaxPages; ++i)
        addChildTop(_pageBtn[i]);
    for (uint8_t i = 0; i < 5u; ++i)
        addChildTop(_viewBtn[i]);
    addChildTop(_link);
    addChildTop(_cycle);
    addChildTop(_chVal);
    addChildTop(_zero);
    addChildTop(_full);
    addChildTop(_blk);
}

void MonitorView::showOn(nex::ovl::Overlay& ovl) noexcept
{
    _overlay = &ovl;
    if (_tester != nullptr)
        _tester->setRows(rows());
    layout();
    syncChrome();
    show(ovl);
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
    present(_grid);
    present(_link);
    present(_cycle);
    present(_chVal);
    present(_univ);
}

void MonitorView::layout() noexcept
{
    const uint8_t nRows = rows();
    const nex::Coord ch = layout::cellH();

    _grid.setRegion(nex::Region(
        nex::Point{0, layout::kTopH},
        nex::Rect{layout::kScreenW, static_cast<nex::Coord>(layout::kColHdrH + ch * nRows)}));

    layoutChrome();
    Widget::layout();
}

void MonitorView::layoutChrome() noexcept
{
    constexpr nex::Coord kY = 4;
    constexpr nex::Coord kH = 40;
    nex::Coord x = layout::kPad;
    auto place = [&](nex::ovl::Object& o, nex::Coord w) {
        o.setRegion(nex::Region(nex::Point{x, kY}, nex::Rect{w, kH}));
        x = static_cast<nex::Coord>(x + w + 8);
    };

    place(_dir, 88);
    place(_rs485, 100);
    place(_art, 110);
    place(_sacn, 88);
    place(_univ, 140);
    place(_net, 88);

    const uint8_t nPages = pages();
    const nex::Coord fy = layout::footY();
    x = layout::kPad;
    for (uint8_t i = 0; i < kMaxPages; ++i) {
        _pageBtn[i].setVisible(i < nPages);
        if (i < nPages) {
            _pageBtn[i].setRegion(nex::Region(nex::Point{x, static_cast<nex::Coord>(fy + 4)}, nex::Rect{108, 40}));
            x = static_cast<nex::Coord>(x + 116);
        }
    }

    x = static_cast<nex::Coord>(layout::kScreenW - layout::kPad);
    for (int i = 4; i >= 0; --i) {
        x = static_cast<nex::Coord>(x - 100);
        _viewBtn[static_cast<uint8_t>(i)].setRegion(
            nex::Region(nex::Point{x, static_cast<nex::Coord>(fy + 4)}, nex::Rect{96, 40}));
        x = static_cast<nex::Coord>(x - 8);
    }

    const nex::Coord y2 = static_cast<nex::Coord>(fy + 56);
    _link.setRegion(nex::Region(nex::Point{layout::kPad, y2}, nex::Rect{160, 40}));
    _cycle.setRegion(nex::Region(nex::Point{176, y2}, nex::Rect{140, 40}));
    _chVal.setRegion(nex::Region(nex::Point{324, y2}, nex::Rect{220, 40}));
    _zero.setRegion(nex::Region(nex::Point{760, y2}, nex::Rect{56, 40}));
    _full.setRegion(nex::Region(nex::Point{824, y2}, nex::Rect{64, 40}));
    _blk.setRegion(nex::Region(nex::Point{896, y2}, nex::Rect{64, 40}));
}

void MonitorView::syncChrome() noexcept
{
    const uint8_t nRows = rows();
    const uint8_t nPages = pages();
    uint8_t page = 0;
    ViewMode view = ViewMode::Current;
    dmx::Direction dir = dmx::Direction::Receive;
    dmx::Transport tr = dmx::Transport::Rs485;
    uint16_t univ = 0;
    uint32_t cycle = 0;
    const char* link = "—";
    uint16_t sel = 1;
    uint8_t selV = 0;

    if (_tester != nullptr) {
        _tester->setRows(nRows);
        page = _tester->page();
        view = _tester->view();
        dir = _tester->port().direction();
        tr = _tester->port().transport();
        univ = _tester->port().universe();
        cycle = _tester->port().frameCount();
        link = dmx::cstr(_tester->port().getStatus());
        sel = _tester->selected();
        selV = _tester->live().get(sel);
    }

    _dir.setLabel(dir == dmx::Direction::Transmit ? "TX" : "RX");
    applyButtonStyle(_dir, dir == dmx::Direction::Transmit);
    applyButtonStyle(_rs485, tr == dmx::Transport::Rs485);
    applyButtonStyle(_art, tr == dmx::Transport::ArtNet);
    applyButtonStyle(_sacn, tr == dmx::Transport::Sacn);
    applyButtonStyle(_viewBtn[0], view == ViewMode::Current);
    applyButtonStyle(_viewBtn[1], view == ViewMode::Fast);
    applyButtonStyle(_viewBtn[2], view == ViewMode::Log && _tester != nullptr && _tester->logging());
    applyButtonStyle(_viewBtn[3], view == ViewMode::Min);
    applyButtonStyle(_viewBtn[4], view == ViewMode::Max);

    char buf[20]{};
    std::snprintf(buf, sizeof(buf), "U:%u", static_cast<unsigned>(univ));
    _univ.setText(buf);
    _link.setText(link);
    _link.fg = (std::strcmp(link, "OK") == 0 || std::strcmp(link, "—") == 0) ? kOk : kErr;
    std::snprintf(buf, sizeof(buf), "#%lu", static_cast<unsigned long>(cycle));
    _cycle.setText(buf);
    std::snprintf(buf, sizeof(buf), "ch %u = %u", static_cast<unsigned>(sel), static_cast<unsigned>(selV));
    _chVal.setText(buf);

    for (uint8_t i = 0; i < nPages; ++i) {
        const uint16_t a = pageFirst(i, nRows);
        const uint16_t b = pageLast(i, nRows);
        std::snprintf(_pageLabel[i], sizeof(_pageLabel[i]), "%u-%u",
            static_cast<unsigned>(a), static_cast<unsigned>(b));
        _pageBtn[i].setLabel(_pageLabel[i]);
        applyButtonStyle(_pageBtn[i], i == page);
    }
}

void MonitorView::applyButtonStyle(nex::ovl::Button& btn, const bool selected) noexcept
{
    btn.setStyle(selected ? kBtnOn : kBtnIdle);
}

void MonitorView::drawBackground(const nex::AppCanvas& cs) const
{
    cs.rect_fill(screenRegion(), kBg);
    cs.rect_fill(nex::Region(nex::Point{0, 0}, nex::Rect{layout::kScreenW, layout::kTopH}), kChrome);
    cs.rect_fill(nex::Region(nex::Point{0, layout::footY()}, nex::Rect{layout::kScreenW, layout::kFootH}), kChrome);
}

void MonitorView::drawBackgroundRegion(const nex::AppCanvas& cs, const nex::Region clip) const
{
    cs.rect_fill(clip, kBg);
}

void MonitorView::present(nex::ovl::Object& obj) noexcept
{
    if (_overlay == nullptr || !isVisible() || _overlay->isModal())
        return;
    redrawObject(obj, _overlay->app.cs);
}

void MonitorView::onClick(nex::ovl::Object* const target) noexcept
{
    if (target == &_grid) {
        onGridClick();
        return;
    }
    if (target == &_net) {
        goNet();
        return;
    }

    if (_tester == nullptr)
        return;

    if (target == &_dir) {
        const auto next = (_tester->port().direction() == dmx::Direction::Receive)
            ? dmx::Direction::Transmit
            : dmx::Direction::Receive;
        _tester->port().setDirection(next);
    } else if (target == &_zero) {
        _tester->setChannel(_tester->selected(), 0);
        if (_tester->port().direction() == dmx::Direction::Transmit)
            _tester->sendLive();
    } else if (target == &_full) {
        _tester->setChannel(_tester->selected(), 255);
        if (_tester->port().direction() == dmx::Direction::Transmit)
            _tester->sendLive();
    } else if (target == &_blk) {
        _tester->blackout();
        if (_tester->port().direction() == dmx::Direction::Transmit)
            _tester->sendLive();
    } else if (target == &_viewBtn[0]) {
        _tester->setView(ViewMode::Current);
    } else if (target == &_viewBtn[1]) {
        _tester->setView(ViewMode::Fast);
    } else if (target == &_viewBtn[2]) {
        _tester->toggleLog();
        _tester->setView(ViewMode::Log);
    } else if (target == &_viewBtn[3]) {
        _tester->setView(ViewMode::Min);
    } else if (target == &_viewBtn[4]) {
        _tester->setView(ViewMode::Max);
    } else {
        for (uint8_t i = 0; i < pages(); ++i) {
            if (target == &_pageBtn[i]) {
                _tester->setPage(i);
                break;
            }
        }
    }
    refresh();
}

void MonitorView::onGridClick() noexcept
{
    if (!_grid.haveHit || _tester == nullptr)
        return;
    const uint16_t ch = channelOf(_tester->page(), rows(), _grid.hitCol, _grid.hitRow);
    if (ch == 0u)
        return;
    _tester->select(ch);
    present(_chVal);
    present(_grid);
}

void MonitorView::goNet() noexcept
{
    hideFrom(_app.overlay);
    _app.switchPage(_app.net);
}

void MonitorView::Label::setText(const char* const src) noexcept
{
    copyText(text, sizeof(text), src);
}

void MonitorView::Label::draw(const nex::AppCanvas& cs) const
{
    if (!isVisible() || text[0] == '\0')
        return;
    cs.text_in_region(screenRegion(), 4u, text, kFont, fg, align, nex::VAlign::Center, bg, nex::BG::Color);
}

void MonitorView::Grid::draw(const nex::AppCanvas& cs) const
{
    if (host == nullptr || !isVisible())
        return;

    const uint8_t nRows = host->rows();
    const nex::Coord cw = layout::cellW();
    const nex::Region g = screenRegion();
    uint8_t page = 0;
    uint16_t selected = 0;
    Tester* const t = host->_tester;
    if (t != nullptr) {
        page = t->page();
        selected = t->selected();
    }

    for (uint8_t col = 0; col < kCols; ++col) {
        char hdr[8]{};
        std::snprintf(hdr, sizeof(hdr), "%u", static_cast<unsigned>(col + 1u));
        const nex::Region hr(nex::Point{static_cast<nex::Coord>(g.ul.x + col * cw), g.ul.y},
            nex::Rect{cw, layout::kColHdrH});
        cs.text_in_region(hr, hdr, kFont, kText, nex::HAlign::Center, nex::VAlign::Center, kChrome, nex::BG::Color);
    }

    char val[8]{};
    for (uint8_t row = 0; row < nRows; ++row) {
        for (uint8_t col = 0; col < kCols; ++col) {
            const uint16_t chan = channelOf(page, nRows, col, row);
            const nex::Region cell = cellRegion(col, row);
            nex::Color bg = kCellBg;
            nex::Color fg = kCellFg;
            if (chan == 0u) {
                cs.rect_fill(cell, kBg);
                continue;
            }
            uint8_t v = 0;
            bool fast = false;
            if (t != nullptr) {
                v = t->cellValue(col, row);
                fast = (t->view() == ViewMode::Fast) && t->cellChanged(col, row);
            }
            if (fast) {
                bg = kChangedBg;
                fg = kChangedFg;
            }
            if (chan == selected)
                cs.rect_bordered(cell, bg, kSelect, 2u);
            else
                cs.rect_bordered(cell, bg, kCellGrid, 1u);
            std::snprintf(val, sizeof(val), "%u", static_cast<unsigned>(v));
            cs.text_in_region(cell, val, kFont, fg, nex::HAlign::Center, nex::VAlign::Center, bg, nex::BG::Transparent);
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
            static_cast<nex::Coord>(g.ul.x + static_cast<nex::Coord>(col) * cw),
            static_cast<nex::Coord>(g.ul.y + layout::kColHdrH + static_cast<nex::Coord>(row) * ch)},
        nex::Rect{cw, ch});
}

bool MonitorView::Grid::onTouchXY(const nex::msg::evTouchXY& e) noexcept
{
    if (host == nullptr || e.state != nex::TouchState::Press)
        return true;

    haveHit = false;
    const uint8_t nRows = host->rows();
    for (uint8_t row = 0; row < nRows; ++row) {
        for (uint8_t col = 0; col < kCols; ++col) {
            if (cellRegion(col, row).contains(e.pos)) {
                hitCol = col;
                hitRow = row;
                haveHit = true;
                return true;
            }
        }
    }
    return true;
}

} // namespace ui
