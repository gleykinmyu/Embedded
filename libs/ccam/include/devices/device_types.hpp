/**
 * @file device_types.hpp
 * @brief Идентификаторы моделей из ConvertibleProtocol.pdf v3.05.
 */

#pragma once

#include <cstdint>

namespace ccam::devices {

/** PTZ-камера (camera protocol). */
enum class CameraModelId : uint8_t {
    Generic,
    He130,
    He120,
    He60,
    He50,
    Ue150,
    Ue155,
    Un145,
    He870,
    Hr140,
    Ub300,
    /** Студийные AW-E600 / AW-E600E. */
    E600,
    /** Студийные AW-E650 / AW-E650E. */
    E650,
};

/** Поворотное устройство / P/T-часть PTZ (P/T protocol). */
enum class PtModelId : uint8_t {
    Generic,
    /** Встроенный pan/tilt в PTZ-камере (HE/UE/HR…). */
    IntegratedPtz,
    /** Отдельная головка AW-PH350 / PH350E и серия PH. */
    Ph350,
    Ph360,
    Ph650,
    Ph405,
};

} // namespace ccam::devices
