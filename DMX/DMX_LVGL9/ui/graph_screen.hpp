/**
 * @file graph_screen.hpp
 * @brief Экран графика выбранных каналов (до kMaxSelect = 8).
 *
 * Собирается лениво из App::showGraph(). Таймер сэмплирует трассу
 * через Tester::sampleTrace() и дорисовывает только новые точки.
 */
#pragma once

#include "chrome.hpp"
#include "layout.hpp"
#include "tester.hpp"

#include <optional>

#include <lv/lv.hpp>

namespace ui {

class GraphScreen : public lv::Component<GraphScreen> {
public:
    explicit GraphScreen(Tester& tester) noexcept
        : _tester(tester)
    {
    }

    lv::ObjectView build(lv::ObjectView parent);
    void on_show() noexcept; ///< Сброс трассы/кешей при каждом входе на экран
    void pause() noexcept;
    void resume() noexcept;

private:
    static constexpr uint8_t kGridY = 4u;  ///< Горизонтальные деления Y
    static constexpr uint8_t kGridX = 10u; ///< Вертикальные деления X
    static constexpr uint8_t kYLabelN = kGridY + 1u;
    static constexpr uint8_t kXLabelN = 6u; ///< Подписи времени (каждый 2-й tick + край)

    void syncChrome() noexcept;
    void syncAxisLabels() noexcept;
    void syncChart(bool force) noexcept;
    void rebuildSeries() noexcept; ///< Показать/скрыть series и легенду по selectedCount

    void on_tick() noexcept;
    void on_back(lv::Event) noexcept;
    void on_scale(lv::Event) noexcept;
    void on_span_prev(lv::Event) noexcept;
    void on_span_next(lv::Event) noexcept;
    void on_band_prev(lv::Event) noexcept;
    void on_band_next(lv::Event) noexcept;
    void on_link(lv::Event) noexcept;

    Tester& _tester;
    lv::Timer _timer{};

    lv::Chart _chart{};
    lv_chart_series_t* _series[kMaxSelect]{};
    int32_t _yBuf[kMaxSelect][kTraceLen]{}; ///< Внешний буфер точек (LV_CHART_POINT_NONE = gap)

    lv::Box _legDot[kMaxSelect]{};
    ChromeLabel _legend[kMaxSelect]{};
    ChromeLabel _yLbl[kYLabelN]{};
    ChromeLabel _xLbl[kXLabelN]{};

    LinkBadge _linkBadge{};
    ArrowNav _spanNav{}; ///< Окно времени трассы (10…100 s)
    ArrowNav _bandNav{}; ///< Полоса Y (Full / 0–64 / …)
    ChromeBtn _scale{};
    ChromeBtn _back{};

    int32_t _chartX = 0;
    int32_t _chartY = 0;
    int32_t _chartW = 0;
    int32_t _chartH = 0;

    uint16_t _traceGen = 0;  ///< Кеш Tester::traceGen() — полный redraw при сбросе
    uint16_t _drawnLen = 0;  ///< Сколько точек уже скопировано в _yBuf
    std::optional<uint8_t> _selN;
    std::optional<uint8_t> _spanShown;
    std::optional<ValueScale> _scaleShown;
    std::optional<YBand> _bandShown;
};

} // namespace ui
