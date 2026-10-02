/**
 * @file layout.hpp
 * @brief Геометрия 4.3" landscape 480×272: сетка 10×4.
 */
#pragma once

#include "UI/cellMap.hpp"
#include "nex.hpp"

namespace ui {
namespace layout {

inline constexpr nex::Coord kScreenW = 480;
inline constexpr nex::Coord kScreenH = 272;
inline constexpr nex::Coord kTopH = 0;
inline constexpr nex::Coord kColHdrH = 22;
inline constexpr nex::Coord kRowHdrW = 48;
inline constexpr nex::Coord kFootH = 72;
inline constexpr nex::Coord kGraphFootH = 36;
inline constexpr nex::Coord kPad = 4;
inline constexpr nex::Coord kBtnH = 28;
inline constexpr nex::Coord kGap = 4;
inline constexpr nex::Coord kCellInset = 1;
inline constexpr nex::Coord kSelPad = 3;
inline constexpr nex::Coord kPipW = 16;
inline constexpr nex::Coord kLinkW = 124;
inline constexpr nex::Coord kNavW = 28;
inline constexpr nex::Coord kArrowGap = 1;
inline constexpr nex::Coord kClearW = 64;
inline constexpr nex::Coord kViewW = 96;
inline constexpr nex::Coord kScaleW = 48;
inline constexpr nex::Coord kYBandW = 64;
inline constexpr nex::Coord kActionW = 72;
inline constexpr nex::Coord kPageRangeW = 80;
inline constexpr nex::Coord kSpanLabelW = 44;
inline constexpr nex::Coord kGraphRuleH = 2;

[[nodiscard]] constexpr nex::Coord cellW() noexcept
{
    return static_cast<nex::Coord>((kScreenW - kRowHdrW) / static_cast<nex::Coord>(kCols));
}

[[nodiscard]] constexpr nex::Coord gridY() noexcept
{
    return static_cast<nex::Coord>(kTopH + kColHdrH);
}

[[nodiscard]] constexpr nex::Coord gridAvailH() noexcept
{
    return static_cast<nex::Coord>(kScreenH - kTopH - kColHdrH - kFootH);
}

[[nodiscard]] constexpr uint8_t rowsFit() noexcept
{
    return kRows;
}

[[nodiscard]] constexpr nex::Coord cellH() noexcept
{
    const uint8_t rows = rowsFit();
    return static_cast<nex::Coord>(gridAvailH() / static_cast<nex::Coord>(rows));
}

[[nodiscard]] constexpr nex::Coord gridW() noexcept
{
    return static_cast<nex::Coord>(cellW() * static_cast<nex::Coord>(kCols));
}

[[nodiscard]] constexpr nex::Coord gridH() noexcept
{
    return static_cast<nex::Coord>(cellH() * static_cast<nex::Coord>(rowsFit()));
}

[[nodiscard]] constexpr nex::Coord gridOriginX() noexcept
{
    return static_cast<nex::Coord>(kRowHdrW + kCellInset);
}

[[nodiscard]] constexpr nex::Coord gridOriginY() noexcept
{
    return static_cast<nex::Coord>(gridY() + kCellInset);
}

[[nodiscard]] constexpr nex::Coord gridFillW() noexcept
{
    return static_cast<nex::Coord>(gridW() + 2 * kCellInset);
}

[[nodiscard]] constexpr nex::Coord gridFillH() noexcept
{
    return static_cast<nex::Coord>(gridH() + 2 * kCellInset);
}

[[nodiscard]] constexpr nex::Coord gridRight() noexcept
{
    return static_cast<nex::Coord>(kRowHdrW + gridFillW());
}

[[nodiscard]] constexpr nex::Coord footY() noexcept
{
    return static_cast<nex::Coord>(kScreenH - kFootH);
}

[[nodiscard]] constexpr nex::Coord graphFootY() noexcept
{
    return static_cast<nex::Coord>(kScreenH - kGraphFootH);
}

[[nodiscard]] constexpr nex::Coord actionX() noexcept
{
    return static_cast<nex::Coord>(kScreenW - kPad - kActionW);
}

[[nodiscard]] constexpr nex::Coord viewX() noexcept
{
    return static_cast<nex::Coord>(actionX() - kGap - kViewW);
}

[[nodiscard]] constexpr nex::Coord scaleX() noexcept
{
    return static_cast<nex::Coord>(viewX() - kGap - kScaleW);
}

[[nodiscard]] constexpr nex::Coord graphScaleX() noexcept
{
    return static_cast<nex::Coord>(actionX() - kGap - kScaleW);
}

[[nodiscard]] constexpr nex::Coord arrowGroupW(const nex::Coord labelW) noexcept
{
    return static_cast<nex::Coord>(kNavW + kArrowGap + labelW + kArrowGap + kNavW);
}

[[nodiscard]] constexpr nex::Coord graphYBandBoxW() noexcept
{
    return arrowGroupW(kYBandW);
}

[[nodiscard]] constexpr nex::Coord graphSpanBoxW() noexcept
{
    return arrowGroupW(kSpanLabelW);
}

[[nodiscard]] constexpr nex::Coord graphYBandPrevX() noexcept
{
    return static_cast<nex::Coord>(graphScaleX() - kGap - graphYBandBoxW());
}

[[nodiscard]] constexpr nex::Coord graphSpanPrevX() noexcept
{
    return static_cast<nex::Coord>(graphYBandPrevX() - graphSpanBoxW());
}

/** Нижний ряд статуса: одинаковый Y на мониторе и графике. */
[[nodiscard]] constexpr nex::Coord statusRowY() noexcept
{
    return static_cast<nex::Coord>(kScreenH - kPad - kBtnH);
}

[[nodiscard]] constexpr nex::Coord pageBoxW() noexcept
{
    return static_cast<nex::Coord>(2 + kSelPad + kNavW + kGap + kPageRangeW + kGap + kNavW + kSelPad);
}

} // namespace layout
} // namespace ui
