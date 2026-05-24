/**
 * @file pt_types.hpp
 * @brief Типы P/T Control Protocol (#…CR).
 */

#pragma once

#include <cstdint>

namespace ccam {

/** Режим питания (#O). Значения — ASCII '0'..'3' в кадре. */
enum class PtPowerMode : uint8_t {
    Off = 0,
    On = 1,
    OnWithCameraTx = 2,
    OnWithoutCameraTx = 3,
};

/** Направление движения по оси (для movePan / moveTilt / …). */
enum class PtAxisDir : uint8_t {
    Left,
    Right,
    Down,
    Up,
    Wide,
    Tele,
    Near,
    Far,
};

/** Ответ на запрос #PTV (pTV…). */
struct PtPosition {
    uint16_t pan = 0;
    uint16_t tilt = 0;
    uint16_t zoom = 0;
    uint16_t focus = 0;
    uint16_t iris = 0;
};

/** Ответ #PTG — gain/shutter/ND (hex поля, см. PDF). */
struct PtExposureInfo {
    uint16_t gain = 0;
    uint16_t shutter = 0;
    uint16_t nd = 0;
};

} // namespace ccam
