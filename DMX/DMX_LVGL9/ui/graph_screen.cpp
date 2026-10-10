/**
 * @file graph_screen.cpp
 * @brief Сборка и обновление экрана графика.
 *
 * Легенда сверху без рамок; chart с pad=0 — крайние div совпадают с border.
 * Ось Y слева и время снизу позиционируются относительно plot (внутри border).
 */

#include "graph_screen.hpp"

#include "colors.hpp"
#include "layout.hpp"
#include "ui.hpp"
#include "util.hpp"

#include <cstdio>
#include <cstring>

namespace ui {

namespace {

constexpr uint32_t kRefreshMs = 100u;

#ifndef LV_CHART_POINT_NONE
#define LV_CHART_POINT_NONE (INT32_MAX)
#endif

} // namespace

lv::ObjectView GraphScreen::build(lv::ObjectView parent)
{
    auto root = makeScreenRoot(parent);

    const int32_t legH = layout::kGraphLegH;
    const int32_t legItemW = (layout::kScreenW - 2 * layout::kPad) / kMaxSelect;
    const int32_t dot = layout::kGraphLegDot;
    const int32_t nameW = legItemW - (dot + layout::kGap / 2);

    for (uint8_t i = 0; i < kMaxSelect; ++i) {
        const int32_t x = layout::kPad + i * legItemW;
        _legDot[i] = lv::Box::create(root)
                         .size(dot, dot)
                         .radius(2)
                         .bg_color(colors::trace(i))
                         .bg_opa(LV_OPA_COVER)
                         .border_width(0)
                         .align(LV_ALIGN_TOP_LEFT, x, (legH - dot) / 2);
        lv_obj_set_hidden(_legDot[i].get(), true);

        _legend[i]
            .create(root, nameW, legH, "")
            .text_left(4)
            .text_color(colors::trace(i))
            .align(LV_ALIGN_TOP_LEFT, x + dot + layout::kGap / 2, 0);
        lv_obj_set_hidden(_legend[i].slot().get(), true);
    }

    _chartX = layout::kGraphAxisW;
    _chartY = legH + layout::kGraphLegGap;
    _chartW = layout::kScreenW - layout::kGraphAxisW - layout::kChartRightGutter;
    _chartH = layout::graphFootY() - _chartY - layout::kGraphTimeH - layout::kChartPad;

    for (uint8_t i = 0; i < kYLabelN; ++i) {
        _yLbl[i].create(root, layout::kGraphAxisW - layout::kChartPad, layout::kGraphYLblH, "");
        lv_obj_set_style_text_align(_yLbl[i].label().get(), LV_TEXT_ALIGN_RIGHT, 0);
    }

    _chart = lv::Chart::create(root)
                 .size(_chartW, _chartH)
                 .align(LV_ALIGN_TOP_LEFT, _chartX, _chartY)
                 .type_line()
                 .point_count(kTraceLen)
                 .update_shift()
                 .div_lines(kYLabelN, kGridX + 1u)
                 .range_y(0, 255)
                 .bg_color(colors::bg())
                 .bg_opa(LV_OPA_COVER)
                 .border_width(1)
                 .border_color(colors::border())
                 // pad=0: LVGL не дублирует крайние div-линии — они совпадают с рамкой
                 .padding(0);
    lv_obj_set_style_line_width(_chart.get(), layout::kSeriesLineW, LV_PART_ITEMS);
    lv_obj_set_style_line_color(_chart.get(), colors::border(), LV_PART_MAIN);
    lv_obj_set_style_line_width(_chart.get(), 1, LV_PART_MAIN);
    lv_obj_set_style_size(_chart.get(), 0, 0, LV_PART_INDICATOR);
    lv_obj_set_scrollbar_mode(_chart.get(), LV_SCROLLBAR_MODE_OFF);

    for (uint8_t i = 0; i < kMaxSelect; ++i) {
        _series[i] = _chart.add_series(colors::trace(i), LV_CHART_AXIS_PRIMARY_Y);
        _chart.set_series_ext_y_array(_series[i], _yBuf[i]);
        _chart.hide_series(_series[i], true);
        for (uint16_t p = 0; p < kTraceLen; ++p)
            _yBuf[i][p] = LV_CHART_POINT_NONE;
    }

    for (uint8_t i = 0; i < kXLabelN; ++i)
        _xLbl[i].create(root, layout::kGraphXLblW, layout::kGraphTimeH, "");

    auto foot = makeChromeFootBar(root, layout::statusRowY(), layout::kGap).space_between();
    _linkBadge.create(foot, layout::kLinkW);
    _linkBadge.on_click<&GraphScreen::on_link>(this);

    auto controls = makeChromeBar(foot, layout::kGap);
    _spanNav.create(controls, layout::kSpanLabelW, "20s");
    _spanNav.bind<&GraphScreen::on_span_prev, &GraphScreen::on_span_next>(this);
    _bandNav.create(controls, layout::kYBandW, "0-255");
    _bandNav.bind<&GraphScreen::on_band_prev, &GraphScreen::on_band_next>(this);
    _scale.create(controls, layout::kScaleW, layout::kBtnH, "DMX")
        .on_click<&GraphScreen::on_scale>(this);
    _back.create(controls, layout::kActionW, layout::kBtnH, "Grid")
        .on_click<&GraphScreen::on_back>(this);

    syncAxisLabels();

    _timer = lv::Timer::create<&GraphScreen::on_tick>(kRefreshMs, this);
    _timer.pause(); // стартует только в on_show()
    return root;
}

void GraphScreen::on_show() noexcept
{
    _tester.clearTrace();
    _selN.reset();
    _traceGen = 0;
    _drawnLen = 0;
    _spanShown.reset();
    _scaleShown.reset();
    _bandShown.reset();
    _linkBadge.invalidate();
    for (uint8_t s = 0; s < kMaxSelect; ++s)
        for (uint16_t i = 0; i < kTraceLen; ++i)
            _yBuf[s][i] = LV_CHART_POINT_NONE;
    rebuildSeries();
    syncChrome();
    syncAxisLabels();
    syncChart(true);
    resume();
}

void GraphScreen::pause() noexcept
{
    if (_timer)
        _timer.pause();
}

void GraphScreen::resume() noexcept
{
    if (_timer)
        _timer.resume();
}

void GraphScreen::rebuildSeries() noexcept
{
    const uint8_t n = _tester.selectedCount();
    _selN = n;
    for (uint8_t i = 0; i < kMaxSelect; ++i) {
        const bool on = i < n;
        _chart.hide_series(_series[i], !on);
        if (on) {
            char buf[12]{};
            std::snprintf(buf, sizeof(buf), "ch %u", static_cast<unsigned>(_tester.selectedAt(i)));
            _legend[i].set_text(buf).text_color(colors::trace(i));
            _legDot[i].bg_color(colors::trace(i));
            lv_obj_set_hidden(_legDot[i].get(), false);
            lv_obj_set_hidden(_legend[i].slot().get(), false);
        } else {
            _legend[i].set_text("");
            lv_obj_set_hidden(_legDot[i].get(), true);
            lv_obj_set_hidden(_legend[i].slot().get(), true);
        }
    }
}

void GraphScreen::syncAxisLabels() noexcept
{
    const uint8_t lo = _tester.yLo();
    const uint8_t hi = (_tester.yHi() <= lo) ? static_cast<uint8_t>(lo + 1u) : _tester.yHi();
    // Контент chart = внутри border (1px); крайние деления = рамка
    constexpr int32_t kBorder = 1;
    const int32_t plotTop = _chartY + kBorder;
    const int32_t plotH = _chartH - 2 * kBorder;
    if (plotH <= 1)
        return;

    const int32_t yHalf = layout::kGraphYLblH / 2;
    char buf[8]{};
    for (uint8_t i = 0; i < kYLabelN; ++i) {
        const uint8_t raw = (i >= kGridY)
            ? lo
            : static_cast<uint8_t>(hi - static_cast<uint16_t>(hi - lo) * i / kGridY);
        _tester.formatValue(raw, buf, sizeof(buf));
        _yLbl[i].set_text(buf);
        const int32_t y = plotTop + (plotH - 1) * static_cast<int32_t>(i) / static_cast<int32_t>(kGridY) - yHalf;
        _yLbl[i].align(LV_ALIGN_TOP_LEFT, layout::kPad / 4, y);
    }

    const uint8_t spanS = _tester.traceSpanS();
    const int32_t plotLeft = _chartX + kBorder;
    const int32_t plotW = _chartW - 2 * kBorder;
    const int32_t yX = _chartY + _chartH + 1;
    const int32_t xHalf = layout::kGraphXLblW / 2;
    uint8_t li = 0;
    for (uint8_t i = 0; i <= kGridX && li < kXLabelN; ++i) {
        if ((i % 2u) != 0u && i != kGridX)
            continue; // каждый 2-й tick + правый край
        const uint16_t s = static_cast<uint16_t>(static_cast<uint32_t>(i) * spanS / kGridX);
        std::snprintf(buf, sizeof(buf), "%us", static_cast<unsigned>(s));
        _xLbl[li].set_text(buf);
        const int32_t x =
            plotLeft + (plotW - 1) * static_cast<int32_t>(i) / static_cast<int32_t>(kGridX) - xHalf;
        _xLbl[li].align(LV_ALIGN_TOP_LEFT, x, yX);
        ++li;
    }
    for (; li < kXLabelN; ++li)
        _xLbl[li].set_text("");
}

void GraphScreen::syncChrome() noexcept
{
    _linkBadge.sync(_tester);

    const uint8_t span = _tester.traceSpanS();
    const bool spanChanged = !_spanShown || *_spanShown != span;
    if (spanChanged) {
        _spanShown = span;
        char buf[8]{};
        std::snprintf(buf, sizeof(buf), "%us", static_cast<unsigned>(span));
        _spanNav.setText(buf);
    }

    const ValueScale scale = _tester.scale();
    const YBand band = _tester.yBand();
    const bool bandChanged = !_bandShown || *_bandShown != band;
    const bool scaleChanged = !_scaleShown || *_scaleShown != scale;
    if (bandChanged || scaleChanged) {
        _bandShown = band;
        char buf[16]{};
        _tester.formatYBand(buf, sizeof(buf));
        _bandNav.setText(buf);
        const int32_t lo = _tester.yLo();
        const int32_t hi = (_tester.yHi() <= _tester.yLo()) ? lo + 1 : _tester.yHi();
        _chart.range_y(lo, hi);
    }
    if (scaleChanged) {
        _scaleShown = scale;
        _scale.set_text(scale == ValueScale::Percent ? "%" : "DMX").style(scale == ValueScale::Percent);
    }

    if (spanChanged || bandChanged || scaleChanged)
        syncAxisLabels();
}

void GraphScreen::syncChart(bool force) noexcept
{
    const uint8_t n = _tester.selectedCount();
    if (!_selN || *_selN != n) {
        rebuildSeries();
        force = true;
    }

    const uint16_t gen = _tester.traceGen();
    const uint16_t len = _tester.traceLen();

    // Инкрементально: только хвост новых точек
    if (!force && gen == _traceGen && len >= _drawnLen) {
        if (len == _drawnLen)
            return;
        for (uint16_t i = _drawnLen; i < len; ++i) {
            for (uint8_t s = 0; s < *_selN; ++s)
                _yBuf[s][i] = _tester.traceAt(s, i);
        }
        _drawnLen = len;
        lv_chart_refresh(_chart.get());
        return;
    }

    // Полный redraw (смена gen / force / укорочение буфера)
    _traceGen = gen;
    _drawnLen = len;
    for (uint8_t s = 0; s < *_selN; ++s) {
        for (uint16_t i = 0; i < kTraceLen; ++i)
            _yBuf[s][i] = (i < len) ? static_cast<int32_t>(_tester.traceAt(s, i)) : LV_CHART_POINT_NONE;
    }
    for (uint8_t s = *_selN; s < kMaxSelect; ++s) {
        for (uint16_t i = 0; i < kTraceLen; ++i)
            _yBuf[s][i] = LV_CHART_POINT_NONE;
    }
    lv_chart_refresh(_chart.get());
}

void GraphScreen::on_tick() noexcept
{
    _tester.sampleTrace(nowMs());
    syncChrome();
    syncChart(false);
}

void GraphScreen::on_back(lv::Event) noexcept
{
    App::instance().showMonitor();
}

void GraphScreen::on_scale(lv::Event) noexcept
{
    _tester.toggleScale();
    _scaleShown.reset();
    syncChrome();
}

void GraphScreen::on_span_prev(lv::Event) noexcept
{
    _tester.stepTraceSpan(-1);
    _spanShown.reset();
    syncChrome();
    syncChart(true);
}

void GraphScreen::on_span_next(lv::Event) noexcept
{
    _tester.stepTraceSpan(1);
    _spanShown.reset();
    syncChrome();
    syncChart(true);
}

void GraphScreen::on_band_prev(lv::Event) noexcept
{
    _tester.stepYBand(-1);
    _bandShown.reset();
    syncChrome();
    syncChart(true);
}

void GraphScreen::on_band_next(lv::Event) noexcept
{
    _tester.stepYBand(1);
    _bandShown.reset();
    syncChrome();
    syncChart(true);
}

void GraphScreen::on_link(lv::Event) noexcept
{
    _tester.clearErrors();
    _linkBadge.invalidate();
    syncChrome();
}

} // namespace ui
