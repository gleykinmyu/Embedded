/**
 * @file joystick.hpp
 * @brief Состояние джойстика пульта (ADC / UART / CAN — реализуйте IJoystick).
 */

#pragma once

#include "transport/types.hpp"

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
 * Преобразование оси в код скорости P/T (01..49 / 50 / 51..99).
 * @param axis      Значение оси.
 * @param max_rate  Максимальный rate при полном отклонении (1..49).
 * @param deadzone  Мёртвая зона вокруг нуля.
 */
inline uint8_t axisToPtSpeed(int16_t axis, uint8_t max_rate, int16_t deadzone = 80)
{
    if (axis > -deadzone && axis < deadzone) {
        return ccam::kPtSpeedStop;
    }

    int16_t mag = (axis < 0) ? static_cast<int16_t>(-axis) : axis;
    if (mag > 1000) {
        mag = 1000;
    }
    if (max_rate < 1) {
        max_rate = 1;
    }
    if (max_rate > 49) {
        max_rate = 49;
    }

    const uint8_t rate = static_cast<uint8_t>(1 + (mag * (max_rate - 1)) / 1000);

    if (axis < 0) {
        return rate;
    }
    return static_cast<uint8_t>(ccam::kPtSpeedMax - rate + 1);
}

class IJoystick {
public:
    virtual ~IJoystick() = default;
    virtual JoystickAxes poll() = 0;
};

} // namespace app
