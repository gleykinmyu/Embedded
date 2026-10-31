/**
 * @file pt_types.hpp
 * @brief Значения Convertible Protocol (камера + P/T) и связанные типы.
 */

#pragma once

#include "devices/ipt_device.hpp"
#include "protocol/choice_lists.hpp"

#include <cstdint>

namespace ccam {

/** Бинарные 0/1 Off/On (Oxx:n, #TAE, #OPT, #LPC, #WAS, #WLC, …). */
enum class Switch : uint8_t {
    CCAM_FOR_EACH_SWITCH(CCAM_ENUM_SW)
};

/** Field / Frame (Control OFR, reply OFF). */
enum class FieldFrameMode : uint8_t {
    CCAM_FOR_EACH_FIELD_FRAME(CCAM_ENUM)
};

/** DCB — Camera / Color Bar (E650/HE: только 0/1). */
enum class ColorBarMode : uint8_t {
    CCAM_FOR_EACH_COLOR_BAR(CCAM_ENUM)
};

/** ORS — Iris Auto / Manual. */
enum class IrisAutoManual : uint8_t {
    CCAM_FOR_EACH_IRIS_AUTO(CCAM_ENUM)
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

/** #D2 ND filter на встроенном PTZ. Те же коды, что He870NdFilter. */
enum class PtNdFilterMode : uint8_t {
    CCAM_FOR_EACH_ND(CCAM_ENUM)
};

/** #RT — траектория к пресету. */
enum class PrMode : uint8_t {
    CCAM_FOR_EACH_PR_MODE(CCAM_ENUM)
};

/** #PST / поле speed table в #APS/#RPS. */
enum class PrSpeed : uint8_t {
    CCAM_FOR_EACH_PR_SPEED(CCAM_ENUM)
};

/**
 * #LC Data2 / ответ #L (l0/l1): Release или Set.
 * #L1..4 — toggle лимита оси в текущей позиции (не 0/1!).
 */
enum class Limitation : uint8_t {
    CCAM_FOR_EACH_LIMITATION(CCAM_ENUM)
};

/** #L / #LC Data1 — ось лимита. */
enum class LimitAxis : uint8_t {
    CCAM_FOR_EACH_LIMIT_AXIS(CCAM_ENUM)
};

/** #AGL — Tilt Range. */
enum class TiltRange : uint8_t {
    CCAM_FOR_EACH_TILT_RANGE(CCAM_ENUM)
};

/** #FAN / #FA2. Не OSD Fan. */
enum class FanMode : uint8_t {
    CCAM_FOR_EACH_FAN(CCAM_ENUM)
};

/** #WIP — Wiper. */
enum class WiperMode : uint8_t {
    CCAM_FOR_EACH_WIPER(CCAM_ENUM)
};

/** #N — Landing Setting. */
enum class LandingMode : uint8_t {
    CCAM_FOR_EACH_LANDING(CCAM_ENUM)
};

CCAM_DEFINE_SW_LABEL(Switch, CCAM_FOR_EACH_SWITCH)
CCAM_DEFINE_U8_LABEL(FieldFrameMode, CCAM_FOR_EACH_FIELD_FRAME)
CCAM_DEFINE_U8_LABEL(ColorBarMode, CCAM_FOR_EACH_COLOR_BAR)
CCAM_DEFINE_U8_LABEL(IrisAutoManual, CCAM_FOR_EACH_IRIS_AUTO)
CCAM_DEFINE_U8_LABEL(PtNdFilterMode, CCAM_FOR_EACH_ND)
CCAM_DEFINE_U8_LABEL(PrMode, CCAM_FOR_EACH_PR_MODE)
CCAM_DEFINE_U8_LABEL(PrSpeed, CCAM_FOR_EACH_PR_SPEED)
CCAM_DEFINE_U8_LABEL(Limitation, CCAM_FOR_EACH_LIMITATION)
CCAM_DEFINE_U8_LABEL(LimitAxis, CCAM_FOR_EACH_LIMIT_AXIS)
CCAM_DEFINE_U8_LABEL(TiltRange, CCAM_FOR_EACH_TILT_RANGE)
CCAM_DEFINE_U8_LABEL(FanMode, CCAM_FOR_EACH_FAN)
CCAM_DEFINE_U8_LABEL(WiperMode, CCAM_FOR_EACH_WIPER)
CCAM_DEFINE_U8_LABEL(LandingMode, CCAM_FOR_EACH_LANDING)

/** Подпись ответа #O, включая коды вне команды power(). */
inline const char* powerStatusLabel(uint8_t code)
{
    switch (code) {
        CCAM_FOR_EACH_POWER(CCAM_U8_LABEL_CASE)
    default:
        return "?";
    }
}

} // namespace ccam
