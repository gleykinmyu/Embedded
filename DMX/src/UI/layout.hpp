/**
 * @file layout.hpp
 * @brief Геометрия 4.3" landscape 480×272: сетка 8 клеток в ширину.
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

[[nodiscard]] constexpr nex::Coord footY() noexcept
{
    return static_cast<nex::Coord>(kScreenH - kFootH);
}

[[nodiscard]] constexpr nex::Coord graphFootY() noexcept
{
    return static_cast<nex::Coord>(kScreenH - kGraphFootH);
}

} // namespace layout
} // namespace ui
