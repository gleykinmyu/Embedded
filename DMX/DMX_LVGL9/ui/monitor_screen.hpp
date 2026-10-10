/**
 * @file monitor_screen.hpp
 * @brief Экран монитора: сетка 10×8 + двухрядный chrome-футер.
 *
 * Таймер ~200 ms обновляет chrome и клетки с бюджетом (kCellBudget),
 * чтобы не блокировать LVGL на полном refresh всех 80 ячеек.
 */
#pragma once

#include "chrome.hpp"
#include "grid_row.hpp"
#include "tester.hpp"

#include <optional>

#include <lv/lv.hpp>

namespace ui {

class MonitorScreen : public lv::Component<MonitorScreen> {
public:
    explicit MonitorScreen(Tester& tester) noexcept
        : _tester(tester)
    {
    }

    lv::ObjectView build(lv::ObjectView parent);
    void pause() noexcept;  ///< Остановить таймер (уход на Graph)
    void resume() noexcept; ///< Возобновить таймер (возврат с Graph)

private:
    void syncChrome(bool force) noexcept;
    void syncCells(bool force) noexcept;
    void syncOneCell(uint8_t col, uint8_t row, bool force) noexcept;
    void syncHeaders() noexcept;

    void on_tick() noexcept;
    void on_cell(lv::Event e) noexcept;
    void on_page_prev(lv::Event) noexcept;
    void on_page_next(lv::Event) noexcept;
    void on_clear(lv::Event) noexcept;
    void on_scale(lv::Event) noexcept;
    void on_view(lv::Event) noexcept;
    void on_link(lv::Event) noexcept;
    void on_graph(lv::Event) noexcept;

    Tester& _tester;
    lv::Timer _timer{};

    GridRow _rows[kMaxRows]{};
    ChromeLabel _colH[kCols]{}; ///< Номера колонок 1…10 сверху

    ArrowNav _pageNav{};
    ChromeBtn _clear{};
    ChromeLabel _chVal{}; ///< Список выбранных каналов / «ch N = V»
    LinkBadge _linkBadge{};
    ChromeBtn _view{};
    ChromeBtn _scale{};
    ChromeBtn _graph{};

    char _pageLabel[16]{};
    char _selLabel[36]{};
    std::optional<uint8_t> _hdrPage;       ///< Кеш страницы для syncHeaders
    std::optional<ViewMode> _viewShown;
    std::optional<ValueScale> _scaleShown;
    bool _clearLit = false;
};

} // namespace ui
