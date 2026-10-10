/**
 * @file util.hpp
 * @brief Общие мелкие утилиты UI (время и т.п.).
 */
#pragma once

#include <cstdint>

#include "esp_timer.h"

namespace ui {

/** Монотонные миллисекунды с boot (esp_timer). */
[[nodiscard]] inline uint32_t nowMs() noexcept
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

} // namespace ui
