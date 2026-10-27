/**
 * @file pt_types.hpp
 * @brief Значения Convertible Protocol (камера + P/T) и связанные типы.
 */

#pragma once

#include "devices/ipt_device.hpp"

#include <cstdint>

namespace ccam {

/** Бинарные 0/1 Off/On (Oxx:n, #TAE, #OPT, #LPC, #WAS, #WLC, …). */
enum class Switch : uint8_t {
    Off = 0,
    On = 1,
};

/** OFR — Field / Frame. */
enum class FieldFrameMode : uint8_t {
    Field = 0,
    Frame = 1,
};

/** ORS — Iris Auto / Manual (ConvertibleProtocol: 0=Manual, 1=Auto). */
enum class IrisAutoManual : uint8_t {
    Manual = 0,
    Auto = 1,
};

/** Уровень 0..9 (ODD 3D-DNR, OCG Chroma, …). */
enum class Levels : uint8_t {
    lvl0 = 0,
    lvl1 = 1,
    lvl2 = 2,
    lvl3 = 3,
    lvl4 = 4,
    lvl5 = 5,
    lvl6 = 6,
    lvl7 = 7,
    lvl8 = 8,
    lvl9 = 9,
};

/** #D2 ND filter на встроенном PTZ (типичные значения, см. PDF). */
enum class PtNdFilterMode : uint8_t {
    Clear = 0,
    NdQuarter = 1,
    NdSixteenth = 2,
    NdSixtyFourth = 3,
    AutoNd = 8,
};

/** #RT — траектория к пресету (ConvertibleProtocol). */
enum class PrMode : uint8_t {
    Normal = 0,     ///< Обычный путь
    Diagonal = 1,   ///< Диагональный путь
};

/** #PST / поле speed table в #APS/#RPS. */
enum class PrSpeed : uint8_t {
    Slow = 0,
    Mid = 1,
    Fast = 2,  ///< HIGH в части спецификаций
};

/** #L / #LC Data2 — Limitation Setting / действие. */
enum class Limitation : uint8_t {
    Release = 0,
    Set = 1,
};

/** #LC Data1 — ось/сторона лимита. */
enum class LimitAxis : uint8_t {
    TiltUp = 1,
    TiltDown = 2,
    PanLeft = 3,
    PanRight = 4,
};

/** #AGL — Tilt Range. */
enum class TiltRange : uint8_t {
    Narrow = 0,  ///< 190°
    Wide = 1,    ///< 300°
};

/** #FAN / #FA2. */
enum class FanMode : uint8_t {
    Auto = 0,
    High = 1,
    Mid = 2,
    Low = 3,
};

/** #WIP — Wiper. */
enum class WiperMode : uint8_t {
    Off = 0,
    Fast = 1,
    Slow = 2,
};

/** #N — Landing Setting. */
enum class LandingMode : uint8_t {
    Just = 0,
    Soft = 1,
};

} // namespace ccam
