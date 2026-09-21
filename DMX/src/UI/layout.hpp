/**
 * @file layout.hpp
 * @brief Геометрия 10" landscape 1024×600: сетка 16 клеток в ширину.
 */
#pragma once

#include "UI/cellMap.hpp"
#include "nex.hpp"

namespace ui {
namespace layout {

inline constexpr nex::Coord kScreenW = 1024;
inline constexpr nex::Coord kScreenH = 600;
inline constexpr nex::Coord kTopH = 48;
inline constexpr nex::Coord kColHdrH = 28;
inline constexpr nex::Coord kRowHdrW = 48;
inline constexpr nex::Coord kFootH = 104;
inline constexpr nex::Coord kPad = 8;

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

} // namespace layout
} // namespace ui
