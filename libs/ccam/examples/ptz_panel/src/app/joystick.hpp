/**
 * @file joystick.hpp
 * @brief Состояние джойстика пульта (ADC / UART / CAN — реализуйте IJoystick).
 */

#pragma once

#include <cstdint>

namespace app {

/** Оси джойстика: -1000..0..+1000, 0 — центр. */
struct JoystickAxes {
    int16_t pan = 0;
    int16_t tilt = 0;
    int16_t zoom = 0;
    int16_t focus = 0;
    bool button = false;  ///< Кнопка на ручке (стоп / пресет — по желанию)
};

/**
 * Преобразование оси в проценты для pt::move (−100..+100).
 * @param axis      Значение оси (−1000..1000).
 * @param max_pct   |%| при полном отклонении (1..100).
 * @param deadzone  Мёртвая зона вокруг нуля.
 */
inline int8_t axisToSignedSpeed(int16_t axis, uint8_t max_pct, int16_t deadzone = 80)
{
    if (axis > -deadzone && axis < deadzone) {
        return 0;
    }

    int16_t mag = (axis < 0) ? static_cast<int16_t>(-axis) : axis;
    if (mag > 1000) {
        mag = 1000;
    }
    if (max_pct < 1) {
        max_pct = 1;
    }
    if (max_pct > 100) {
        max_pct = 100;
    }

    const int8_t pct = static_cast<int8_t>(1 + (mag * (max_pct - 1)) / 1000);
    return (axis < 0) ? static_cast<int8_t>(-pct) : pct;
}

class IJoystick {
public:
    virtual ~IJoystick() = default;
    virtual JoystickAxes poll() = 0;
};

} // namespace app
