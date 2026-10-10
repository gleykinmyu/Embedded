/**
 * @file monitor_screen.cpp
 * @brief Сборка и обновление экрана монитора.
 *
 * Футер — два flex-ряда (makeChromeFootBar): page/Clear/selection и
 * link + scale/view/graph (space_between).
 */

#include "monitor_screen.hpp"

#include "colors.hpp"
#include "layout.hpp"
#include "ui.hpp"

#include <cstdio>
#include <cstring>

namespace ui {

namespace {

constexpr uint32_t kRefreshMs = 200u;
constexpr uint8_t kCellBudget = 16u; ///< Макс. реально перерисованных клеток за тик

} // namespace

lv::ObjectView MonitorScreen::build(lv::ObjectView parent)
{
    auto root = makeScreenRoot(parent);

    const int32_t cw = layout::cellW();
    const int32_t gx = layout::gridOriginX();
    const uint8_t nRows = layout::rowsFit();

    for (uint8_t col = 0; col < kCols; ++col) {
        char buf[5]{};
        std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(col + 1u));
        _colH[col]
            .create(root, cw, layout::kColHdrH, buf)
            .align(LV_ALIGN_TOP_LEFT, gx + col * cw, layout::kTopH);
    }

    for (uint8_t row = 0; row < nRows; ++row)
        _rows[row].create<&MonitorScreen::on_cell>(root, row, this);

    // Ряд 1: page | Clear + selection
    auto foot1 = makeChromeFootBar(root, layout::footCtrlY(), layout::kSectionGap);
    _pageNav.create(foot1, layout::kPageRangeW, "1-80");
    _pageNav.bind<&MonitorScreen::on_page_prev, &MonitorScreen::on_page_next>(this);

    auto selGroup = makeChromeBar(foot1, layout::kGap);
    _clear.create(selGroup, layout::kClearW, layout::kBtnH, "Clear")
        .on_click<&MonitorScreen::on_clear>(this);
    _chVal.create(selGroup, layout::kSelW, layout::kBtnH, "-")
        .framed()
        .text_left()
        .text_color(colors::accent());

    // Ряд 2: link слева, действия справа
    auto foot2 = makeChromeFootBar(root, layout::statusRowY(), layout::kGap).space_between();
    _linkBadge.create(foot2, layout::kLinkW);
    _linkBadge.on_click<&MonitorScreen::on_link>(this);

    auto actions = makeChromeBar(foot2, layout::kGap);
    _scale.create(actions, layout::kScaleW, layout::kBtnH, "DMX")
        .on_click<&MonitorScreen::on_scale>(this);
    _view.create(actions, layout::kViewW, layout::kBtnH, "Fast")
        .on_click<&MonitorScreen::on_view>(this);
    _graph.create(actions, layout::kActionW, layout::kBtnH, "Graph")
        .on_click<&MonitorScreen::on_graph>(this);

    _tester.setRows(nRows);
    syncHeaders();
    syncChrome(true);
    syncCells(true);

    _timer = lv::Timer::create<&MonitorScreen::on_tick>(kRefreshMs, this);
    return root;
}

void MonitorScreen::pause() noexcept
{
    if (_timer)
        _timer.pause();
}

void MonitorScreen::resume() noexcept
{
    if (_timer)
        _timer.resume();
}

void MonitorScreen::syncHeaders() noexcept
{
    const uint8_t nRows = _tester.rows();
    const uint8_t page = _tester.page();
    if (_hdrPage && *_hdrPage == page)
        return;
    _hdrPage = page;
    for (uint8_t row = 0; row < nRows; ++row)
        _rows[row].syncHeader(_tester);
}

void MonitorScreen::syncChrome(bool force) noexcept
{
    _linkBadge.sync(_tester, force);

    char buf[36]{};
    _tester.formatSelection(buf, sizeof(buf));
    if (force || std::strcmp(_selLabel, buf) != 0) {
        std::memcpy(_selLabel, buf, sizeof(buf));
        _chVal.set_text(_selLabel);
    }

    const bool lit = _tester.selectedCount() > 0u;
    if (force || lit != _clearLit) {
        _clearLit = lit;
        _clear.style(lit);
    }

    const ViewMode view = _tester.view();
    if (force || !_viewShown || view != *_viewShown) {
        _viewShown = view;
        _view.style(false, view == ViewMode::Fast);
    }

    const ValueScale scale = _tester.scale();
    if (force || !_scaleShown || scale != *_scaleShown) {
        _scaleShown = scale;
        _scale.set_text(scale == ValueScale::Percent ? "%" : "DMX").style(scale == ValueScale::Percent);
    }

    const uint16_t a = pageFirst(_tester.page(), _tester.rows());
    const uint16_t b = pageLast(_tester.page(), _tester.rows());
    std::snprintf(buf, sizeof(buf), "%u-%u", static_cast<unsigned>(a), static_cast<unsigned>(b));
    if (force || std::strcmp(_pageLabel, buf) != 0) {
        std::strncpy(_pageLabel, buf, sizeof(_pageLabel) - 1);
        _pageNav.setText(_pageLabel);
    }
}

void MonitorScreen::syncOneCell(uint8_t col, uint8_t row, bool force) noexcept
{
    if (row >= kMaxRows)
        return;
    (void)_rows[row].syncOne(_tester, col, force);
}

void MonitorScreen::syncCells(bool force) noexcept
{
    const uint8_t nRows = _tester.rows();
    const uint16_t total = static_cast<uint16_t>(nRows) * kCols;
    if (force) {
        for (uint8_t row = 0; row < nRows; ++row)
            _rows[row].syncAll(_tester, true);
        return;
    }

    // Круговой обход с бюджетом: за тик трогаем только изменившиеся клетки
    static uint16_t s_cursor = 0;
    uint8_t budget = kCellBudget;
    for (uint16_t n = 0; n < total && budget > 0; ++n) {
        const uint16_t idx = static_cast<uint16_t>((s_cursor + n) % total);
        const uint8_t row = static_cast<uint8_t>(idx / kCols);
        const uint8_t col = static_cast<uint8_t>(idx % kCols);
        if (_rows[row].syncOne(_tester, col, false))
            --budget;
    }
    s_cursor = static_cast<uint16_t>((s_cursor + kCellBudget) % (total ? total : 1u));
}

void MonitorScreen::on_tick() noexcept
{
    syncChrome(false);
    syncCells(false);
}

void MonitorScreen::on_cell(lv::Event e) noexcept
{
    // Метка внутри клетки: поднимаемся к объекту с user_data
    auto* obj = e.current_target().get();
    if (obj == nullptr)
        obj = e.target().get();
    while (obj != nullptr && lv_obj_get_user_data(obj) == nullptr)
        obj = lv_obj_get_parent(obj);
    if (obj == nullptr)
        return;

    uint8_t col = 0;
    uint8_t row = 0;
    GridRow::unpack(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(obj)), col, row);
    if (row >= _tester.rows() || col >= kCols)
        return;
    if (_rows[row].snap(col).empty())
        return;
    const uint16_t ch = channelOf(_tester.page(), _tester.rows(), col, row);
    if (ch == 0u)
        return;

    const auto r = _tester.toggleSelect(ch);
    if (r == Tester::SelectResult::Full) {
        App::instance().alert("Max 8 channels");
        return;
    }

    syncOneCell(col, row, true);
    // Снять/поставить подсветку selection на остальных клетках
    if (r == Tester::SelectResult::Removed || r == Tester::SelectResult::Added) {
        const uint8_t nRows = _tester.rows();
        for (uint8_t rr = 0; rr < nRows; ++rr) {
            for (uint8_t cc = 0; cc < kCols; ++cc) {
                if (cc == col && rr == row)
                    continue;
                const bool want = !_rows[rr].snap(cc).empty()
                    && _tester.isSelected(channelOf(_tester.page(), nRows, cc, rr));
                if (want != _rows[rr].snap(cc).selected())
                    syncOneCell(cc, rr, true);
            }
        }
    }
    syncChrome(true);
}

void MonitorScreen::on_page_prev(lv::Event) noexcept
{
    _tester.stepPage(-1);
    _hdrPage.reset();
    syncHeaders();
    syncChrome(true);
    syncCells(true);
}

void MonitorScreen::on_page_next(lv::Event) noexcept
{
    _tester.stepPage(1);
    _hdrPage.reset();
    syncHeaders();
    syncChrome(true);
    syncCells(true);
}

void MonitorScreen::on_clear(lv::Event) noexcept
{
    if (_tester.selectedCount() == 0u)
        return;
    _tester.clearSelection();
    syncChrome(true);
    syncCells(true);
}

void MonitorScreen::on_scale(lv::Event) noexcept
{
    _tester.toggleScale();
    syncChrome(true);
    syncCells(true);
}

void MonitorScreen::on_view(lv::Event) noexcept
{
    const ViewMode next = (_tester.view() == ViewMode::Current) ? ViewMode::Fast : ViewMode::Current;
    _tester.setView(next);
    syncChrome(true);
    if (_timer)
        _timer.period(next == ViewMode::Fast ? 120u : 200u);
}

void MonitorScreen::on_link(lv::Event) noexcept
{
    _tester.clearErrors();
    _linkBadge.invalidate();
    syncChrome(true);
}

void MonitorScreen::on_graph(lv::Event) noexcept
{
    App::instance().showGraph();
}

} // namespace ui
