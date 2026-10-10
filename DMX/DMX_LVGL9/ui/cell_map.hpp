/**
 * @file cell_map.hpp
 * @brief Карта каналов сетки монитора и общие enum’ы UI.
 *
 * Нумерация: channel = page * (rows*10) + row * 10 + col + 1  (1…512).
 * На последней странице хвост после 512 → channelOf() == 0 (клетка пустая).
 */
#pragma once

#include <cstdint>

#include "dmx_data.hpp"

namespace ui {

inline constexpr uint8_t kCols = 10u;
inline constexpr uint8_t kRows = 8u; ///< 7″ 800×480 — больше рядов, чем на 4.3″
inline constexpr uint8_t kMaxRows = kRows;
inline constexpr uint8_t kMaxSelect = 8u;     ///< Макс. каналов в Graph / select
inline constexpr uint16_t kTraceLen = 200u;   ///< Точек в буфере трассы
inline constexpr uint8_t kTraceSpanMinS = 10u;
inline constexpr uint8_t kTraceSpanMaxS = 100u;
inline constexpr uint8_t kTraceSpanStepS = 10u;

/** Режим отображения значений на сетке. */
enum class ViewMode : uint8_t {
    Current = 0, ///< Обычное обновление
    Fast = 1,    ///< Подсветка изменившихся клеток
};

/** Шкала значений: сырой DMX 0…255 или проценты. */
enum class ValueScale : uint8_t {
    Dmx = 0,
    Percent = 1,
};

/** Вертикальная полоса Y на графике. */
enum class YBand : uint8_t {
    Full = 0,
    B0 = 1,
    B64 = 2,
    B128 = 3,
    B192 = 4,
};

inline constexpr uint8_t kYBandN = 5u;

[[nodiscard]] constexpr uint8_t toPercent(uint8_t v) noexcept
{
    return static_cast<uint8_t>((static_cast<uint16_t>(v) * 100u + 127u) / 255u);
}

[[nodiscard]] constexpr uint8_t yBandLo(YBand band) noexcept
{
    switch (band) {
    case YBand::B64:
        return 64u;
    case YBand::B128:
        return 128u;
    case YBand::B192:
        return 192u;
    default:
        return 0u;
    }
}

[[nodiscard]] constexpr uint8_t yBandHi(YBand band) noexcept
{
    switch (band) {
    case YBand::B0:
        return 64u;
    case YBand::B64:
        return 128u;
    case YBand::B128:
        return 192u;
    default:
        return 255u;
    }
}

[[nodiscard]] constexpr uint16_t pageSize(uint8_t rows) noexcept
{
    return static_cast<uint16_t>(static_cast<uint16_t>(rows) * kCols);
}

[[nodiscard]] constexpr uint8_t pageCount(uint8_t rows) noexcept
{
    const uint16_t ps = pageSize(rows);
    if (ps == 0u)
        return 1u;
    return static_cast<uint8_t>((dmx::kMaxChannels + ps - 1u) / ps);
}

/** Канал 1…512; 0 — пустая клетка (хвост последней страницы). */
[[nodiscard]] constexpr uint16_t channelOf(uint8_t page, uint8_t rows, uint8_t col, uint8_t row) noexcept
{
    if (col >= kCols || row >= rows)
        return 0u;
    const uint16_t ch = static_cast<uint16_t>(
        static_cast<uint16_t>(page) * pageSize(rows) + static_cast<uint16_t>(row) * kCols + col + 1u);
    return ch > dmx::kMaxChannels ? 0u : ch;
}

[[nodiscard]] constexpr uint16_t pageFirst(uint8_t page, uint8_t rows) noexcept
{
    const uint16_t ch = static_cast<uint16_t>(static_cast<uint16_t>(page) * pageSize(rows) + 1u);
    return ch > dmx::kMaxChannels ? 0u : ch;
}

[[nodiscard]] constexpr uint16_t pageLast(uint8_t page, uint8_t rows) noexcept
{
    uint16_t ch = static_cast<uint16_t>(static_cast<uint16_t>(page + 1u) * pageSize(rows));
    if (ch > dmx::kMaxChannels)
        ch = static_cast<uint16_t>(dmx::kMaxChannels);
    return ch;
}

inline constexpr uint8_t kMaxPages = pageCount(kRows);

} // namespace ui
