/**
 * @file colors.hpp
 * @brief Палитра UI — как у STM32/Nextion DMX-тестера (логика RGB888 для LVGL).
 */
#pragma once

#include <lv/lv.hpp>

namespace ui::colors {

inline lv_color_t bg() noexcept { return lv::rgb(0x101410); }         ///< Фон экрана / клеток
inline lv_color_t chrome() noexcept { return lv::rgb(0x101410); }     ///< Запас под chrome
inline lv_color_t btn() noexcept { return lv::rgb(0x292829); }        ///< Кнопка idle
inline lv_color_t accent() noexcept { return lv::rgb(0xFFA500); }     ///< Выделение / lit
inline lv_color_t changed() noexcept { return lv::rgb(0xE61400); }    ///< Fast-change / error
inline lv_color_t border() noexcept { return lv::rgb(0x525052); }     ///< Рамки
inline lv_color_t text() noexcept { return lv::rgb(0xEEEAEE); }       ///< Обычный текст
inline lv_color_t text_light() noexcept { return lv::rgb(0xFFFFFF); } ///< Текст на accent
inline lv_color_t cell_bg() noexcept { return lv::rgb(0x101410); }   ///< Фон клетки

/** Цвет серии графика i (0…7), как на Nextion. */
inline lv_color_t trace(uint8_t i) noexcept
{
    static constexpr uint32_t kHex[8] = {
        0xFFA500u, // amber
        0xE61429u, // red
        0x24C952u, // green
        0x24B5A5u, // teal
        0x3952E7u, // blue
        0x821DF7u, // violet
        0x8A4100u, // chocolate
        0xFFFFFFu, // white
    };
    return lv::rgb(kHex[i & 7u]);
}

} // namespace ui::colors
