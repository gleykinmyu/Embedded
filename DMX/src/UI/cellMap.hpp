/**
 * @file cellMap.hpp
 * @brief Сетка монитора 4.3": 8 каналов в ряд, 4 ряда, 16 страниц (ровно 512).
 *
 * Канал = page * (rows*8) + row * 8 + col + 1  (1…512).
 */
#pragma once

#include <cstdint>

#include "idmx.hpp"

namespace ui {

inline constexpr uint8_t kCols = 8u;
inline constexpr uint8_t kRows = 4u;
inline constexpr uint8_t kMaxRows = kRows;
inline constexpr uint8_t kMaxPages = 16u;
inline constexpr uint8_t kMaxSelect = 8u;
inline constexpr uint16_t kTraceLen = 200u;
inline constexpr uint8_t kTraceSpanMinS = 10u;
inline constexpr uint8_t kTraceSpanMaxS = 100u;
inline constexpr uint8_t kTraceSpanStepS = 10u;
static_assert(static_cast<uint16_t>(kRows) * kCols * kMaxPages == BIF::dmx::kMaxChannels,
    "4×8×16 must cover 512 channels");

enum class ViewMode : uint8_t {
    Current = 0,
    Fast = 1,
};

[[nodiscard]] constexpr uint16_t pageSize(uint8_t rows) noexcept
{
    return static_cast<uint16_t>(static_cast<uint16_t>(rows) * kCols);
}

[[nodiscard]] constexpr uint8_t pageCount(uint8_t rows) noexcept
{
    const uint16_t ps = pageSize(rows);
    if (ps == 0u)
        return 1u;
    return static_cast<uint8_t>((BIF::dmx::kMaxChannels + ps - 1u) / ps);
}

/// Канал 1…512; 0 если клетка пустая (хвост последней страницы).
[[nodiscard]] constexpr uint16_t channelOf(uint8_t page, uint8_t rows, uint8_t col, uint8_t row) noexcept
{
    if (col >= kCols || row >= rows)
        return 0u;
    const uint16_t ch = static_cast<uint16_t>(
        static_cast<uint16_t>(page) * pageSize(rows) + static_cast<uint16_t>(row) * kCols + col + 1u);
    return ch > BIF::dmx::kMaxChannels ? 0u : ch;
}

[[nodiscard]] constexpr uint16_t pageFirst(uint8_t page, uint8_t rows) noexcept
{
    const uint16_t ch = static_cast<uint16_t>(static_cast<uint16_t>(page) * pageSize(rows) + 1u);
    return ch > BIF::dmx::kMaxChannels ? 0u : ch;
}

[[nodiscard]] constexpr uint16_t pageLast(uint8_t page, uint8_t rows) noexcept
{
    uint16_t ch = static_cast<uint16_t>(static_cast<uint16_t>(page + 1u) * pageSize(rows));
    if (ch > BIF::dmx::kMaxChannels)
        ch = static_cast<uint16_t>(BIF::dmx::kMaxChannels);
    return ch;
}

} // namespace ui
