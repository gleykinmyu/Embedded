/**
 * @file layout.hpp
 * @brief Геометрия Waveshare 7″ landscape 800×480.
 *
 * Константы размеров chrome/сетки/графика + helpers координат.
 * Сетка: 10 колонок × 8 рядов; футер монитора — два ряда кнопок.
 */
#pragma once

#include "cell_map.hpp"

namespace ui {
namespace layout {

inline constexpr int32_t kScreenW = 800;
inline constexpr int32_t kScreenH = 480;

/* --- Шапка сетки (номера колонок / десятки рядов) --- */
inline constexpr int32_t kTopH = 0;
inline constexpr int32_t kColHdrH = 28; ///< Высота подписей 1…10 сверху
inline constexpr int32_t kRowHdrW = 40; ///< Ширина подписей десятков слева

/* --- Футер монитора (page/Clear + link/actions) --- */
inline constexpr int32_t kFootH = 96;       ///< Суммарная высота двух рядов + pad
inline constexpr int32_t kPad = 8;          ///< Внешний отступ экрана / полос
inline constexpr int32_t kGap = 8;          ///< Зазор между соседними контролами
inline constexpr int32_t kSectionGap = 14;  ///< Между группами (page ↔ Clear)
inline constexpr int32_t kBtnH = 36;
inline constexpr int32_t kBtnRadius = 4;
inline constexpr int32_t kCellInset = 1;    ///< Зазор между клетками сетки

/* --- Ширины chrome-элементов --- */
inline constexpr int32_t kPipW = 20;
inline constexpr int32_t kPipSize = 14;
inline constexpr int32_t kLinkW = 140;
inline constexpr int32_t kNavW = 40;        ///< Кнопка < / >
inline constexpr int32_t kArrowGap = 4;     ///< Внутри ArrowNav
inline constexpr int32_t kClearW = 80;
inline constexpr int32_t kSelW = 240;       ///< Рамка списка выбранных каналов
inline constexpr int32_t kSelPad = 8;
inline constexpr int32_t kViewW = 100;
inline constexpr int32_t kScaleW = 56;
inline constexpr int32_t kActionW = 88;     ///< Graph / Grid
inline constexpr int32_t kPageRangeW = 96;

/* --- Экран графика --- */
inline constexpr int32_t kGraphFootH = 52;
inline constexpr int32_t kGraphLegH = 28;       ///< Полоса легенды сверху
inline constexpr int32_t kGraphLegGap = 8;      ///< Легенда ↔ chart
inline constexpr int32_t kGraphAxisW = 48;      ///< Колонка Y-подписей
inline constexpr int32_t kGraphTimeH = 22;      ///< Полоса X-подписей под chart
inline constexpr int32_t kGraphYLblH = 18;
inline constexpr int32_t kGraphXLblW = 44;
inline constexpr int32_t kGraphLegDot = 10;
inline constexpr int32_t kYBandW = 96;          ///< Подпись Y-band в ArrowNav
inline constexpr int32_t kSpanLabelW = 56;      ///< Подпись span (20s …)
inline constexpr int32_t kChartPad = 6;         ///< Зазор chart ↔ time strip
inline constexpr int32_t kChartRightGutter = 10;
inline constexpr int32_t kSeriesLineW = 2;

[[nodiscard]] constexpr int32_t graphFootY() noexcept { return kScreenH - kGraphFootH; }

/** Ширина группы < label >. */
[[nodiscard]] constexpr int32_t arrowGroupW(int32_t labelW) noexcept
{
    return kNavW + kArrowGap + labelW + kArrowGap + kNavW;
}

[[nodiscard]] constexpr int32_t cellW() noexcept
{
    return (kScreenW - kRowHdrW) / static_cast<int32_t>(kCols);
}

[[nodiscard]] constexpr int32_t gridY() noexcept { return kTopH + kColHdrH; }

[[nodiscard]] constexpr int32_t gridAvailH() noexcept
{
    return kScreenH - kTopH - kColHdrH - kFootH;
}

[[nodiscard]] constexpr uint8_t rowsFit() noexcept { return kRows; }

[[nodiscard]] constexpr int32_t cellH() noexcept
{
    return gridAvailH() / static_cast<int32_t>(rowsFit());
}

[[nodiscard]] constexpr int32_t gridW() noexcept
{
    return cellW() * static_cast<int32_t>(kCols);
}

[[nodiscard]] constexpr int32_t gridH() noexcept
{
    return cellH() * static_cast<int32_t>(rowsFit());
}

/** Левый верх клетки (0,0) с учётом inset. */
[[nodiscard]] constexpr int32_t gridOriginX() noexcept { return kRowHdrW + kCellInset; }
[[nodiscard]] constexpr int32_t gridOriginY() noexcept { return gridY() + kCellInset; }

[[nodiscard]] constexpr int32_t footY() noexcept { return kScreenH - kFootH; }
[[nodiscard]] constexpr int32_t footCtrlY() noexcept { return footY() + kPad; } ///< Верхний ряд футера
[[nodiscard]] constexpr int32_t statusRowY() noexcept { return kScreenH - kPad - kBtnH; } ///< Нижний ряд

} // namespace layout
} // namespace ui
