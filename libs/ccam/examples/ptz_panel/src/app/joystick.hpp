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
 * Преобразование оси в signed speed для pt::move (−128..127).
 * @param axis      Значение оси (−1000..1000).
 * @param max_rate  |скорость| при полном отклонении (1..127).
 * @param deadzone  Мёртвая зона вокруг нуля.
 */
inline int8_t axisToSignedSpeed(int16_t axis, uint8_t max_rate, int16_t deadzone = 80)
{
    if (axis > -deadzone && axis < deadzone) {
        return 0;
    }

    int16_t mag = (axis < 0) ? static_cast<int16_t>(-axis) : axis;
    if (mag > 1000) {
        mag = 1000;
    }
    if (max_rate < 1) {
        max_rate = 1;
    }
    if (max_rate > 127) {
        max_rate = 127;
    }

    const int8_t rate = static_cast<int8_t>(1 + (mag * (max_rate - 1)) / 1000);
    return (axis < 0) ? static_cast<int8_t>(-rate) : rate;
}

class IJoystick {
public:
    virtual ~IJoystick() = default;
    virtual JoystickAxes poll() = 0;
};

} // namespace app
