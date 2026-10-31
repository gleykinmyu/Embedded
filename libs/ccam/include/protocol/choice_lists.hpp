/**
 * @file choice_lists.hpp
 * @brief Закрытые наборы Convertible Protocol: имя, код, провод, подпись.
 *
 * Один список разворачивается в enum и в CamChoice / label().
 * Сплошные шкалы (matrix, flare, iris follow) сюда не входят.
 *
 * X(name, value, wire, label)
 *   value — число на шине (его и кладём в enum)
 *   wire  — строка HTTP / data, десятичная запись того же числа
 *   label — подпись UI
 *
 * Switch — пять полей: dec ("0") и hex2 ("00").
 * Gamma / AGC — без enum: X(value, wire, label), wire = два hex.
 */

#pragma once

namespace ccam {

constexpr int choiceDec(const char* s)
{
    int v = 0;
    if (s == nullptr || *s == '\0') {
        return -1;
    }
    for (; *s != '\0'; ++s) {
        if (*s < '0' || *s > '9') {
            return -1;
        }
        v = v * 10 + (*s - '0');
    }
    return v;
}

constexpr int choiceHex(const char* s)
{
    int v = 0;
    if (s == nullptr || *s == '\0') {
        return -1;
    }
    for (; *s != '\0'; ++s) {
        const char c = *s;
        int d = -1;
        if (c >= '0' && c <= '9') {
            d = c - '0';
        } else if (c >= 'A' && c <= 'F') {
            d = c - 'A' + 10;
        } else if (c >= 'a' && c <= 'f') {
            d = c - 'a' + 10;
        }
        if (d < 0) {
            return -1;
        }
        v = (v << 4) | d;
    }
    return v;
}

} // namespace ccam

#define CCAM_ENUM(name, value, wire, label) name = (value),
#define CCAM_ENUM_SW(name, value, dec, hex2, label) name = (value),
#define CCAM_CHOICE(name, value, wire, label) {wire, label},
#define CCAM_HEX_CHOICE(value, wire, label) {wire, label},
#define CCAM_CHOICE_DEC(name, value, dec, hex2, label) {dec, label},
#define CCAM_CHOICE_HEX2(name, value, dec, hex2, label) {hex2, label},
#define CCAM_SW_PAYLOAD_CASE(name, value, dec, hex2, label) case (value): return dec;
#define CCAM_U8_LABEL_CASE(name, value, wire, label) case (value): return label;
#define CCAM_SW_LABEL_CASE(name, value, dec, hex2, label) case (value): return label;
#define CCAM_COUNT_ONE(...) +1

#define CCAM_DEFINE_U8_LABEL(Enum, LIST)                                                           \
    inline const char* label(Enum value)                                                           \
    {                                                                                              \
        switch (static_cast<unsigned>(value)) {                                                   \
            LIST(CCAM_U8_LABEL_CASE)                                                               \
        default:                                                                                   \
            return "?";                                                                            \
        }                                                                                          \
    }

#define CCAM_DEFINE_SW_LABEL(Enum, LIST)                                                           \
    inline const char* label(Enum value)                                                           \
    {                                                                                              \
        switch (static_cast<unsigned>(value)) {                                                   \
            LIST(CCAM_SW_LABEL_CASE)                                                               \
        default:                                                                                   \
            return "?";                                                                            \
        }                                                                                          \
    }

/**
 * OAW студия E650/E600 — value = QAW / UI.
 * PDF: control ≠ question.
 *   OAW set: 0=ATW, 1=AWC A, 2=AWC B, 3=ATW, 4=3200K, 5=5600K
 *   QAW:     0=ATW, 1=—,     2=AWC A, 3=AWC B, 4=3200K, 5=5600K
 * AW-E650E: в меню только эти 5; 6…9 на шине не держатся.
 */
#define CCAM_FOR_EACH_STUDIO_AWC(X)                                                                \
    X(Atw, 0, "0", "ATW")                                                                          \
    X(AwcA, 2, "2", "AWC A")                                                                       \
    X(AwcB, 3, "3", "AWC B")                                                                       \
    X(Preset3200K, 4, "4", "3200K")                                                                \
    X(Preset5600K, 5, "5", "5600K")

namespace ccam {

/** QAW/UI → OAW control. false = неизвестный код. */
inline constexpr bool studioAwcQueryToControl(uint8_t query, uint8_t& control)
{
    switch (query) {
    case 0:
        control = 0;
        return true; // ATW
    case 2:
        control = 1;
        return true; // AWC A
    case 3:
        control = 2;
        return true; // AWC B
    case 4:
        control = 4;
        return true; // 3200K
    case 5:
        control = 5;
        return true; // 5600K
    default:
        return false;
    }
}

} // namespace ccam

/** OAW, HE130 / HE870 / UE150. Var = 5, без 4500/6000/2800. */
#define CCAM_FOR_EACH_HE_AWC(X)                                                                    \
    X(Atw, 0, "0", "0 ATW")                                                                        \
    X(AwcA, 1, "1", "1 AWC A")                                                                     \
    X(AwcB, 2, "2", "2 AWC B")                                                                     \
    X(Preset3200K, 3, "3", "3 3200K")                                                              \
    X(Preset5600K, 4, "4", "4 5600K")                                                              \
    X(Var, 5, "5", "5 VAR")

/**
 * SCENE FILE — коды ответа OSF / UI (0…3).
 * Control XSF шлёт OSF+1 (1…4): PDF «control and question is different».
 */
#define CCAM_FOR_EACH_SCENE(X)                                                                     \
    X(Halogen, 0, "0", "Halogen")                                                                  \
    X(Fluorescent, 1, "1", "Fluorescent")                                                          \
    X(Outdoor, 2, "2", "Outdoor")                                                                  \
    X(User, 3, "3", "User")

/** UE150: те же OSF-коды 0…3, другие имена. XSF = OSF+1. */
#define CCAM_FOR_EACH_UE_SCENE(X)                                                                  \
    X(Scene1, 0, "0", "Scene 1")                                                                   \
    X(Scene2, 1, "1", "Scene 2")                                                                   \
    X(Scene3, 2, "2", "Scene 3")                                                                   \
    X(Scene4, 3, "3", "Scene 4")

/** OSH. value = hex-ниббл, wire = десятичная строка ("10" → 0x0A). */
#define CCAM_FOR_EACH_STUDIO_SHUTTER(X)                                                            \
    X(Off, 0, "0", "Off")                                                                          \
    X(Shutter1_50, 1, "1", "1/50")                                                                 \
    X(Shutter1_60, 2, "2", "1/60")                                                                 \
    X(Shutter1_100_120, 3, "3", "1/100.120")                                                       \
    X(Shutter1_120_100, 4, "4", "1/120.100")                                                       \
    X(Shutter1_250, 5, "5", "1/250")                                                               \
    X(Shutter1_500, 6, "6", "1/500")                                                               \
    X(Shutter1_1000, 7, "7", "1/1000")                                                             \
    X(Shutter1_2000, 8, "8", "1/2000")                                                             \
    X(Shutter1_4000, 9, "9", "1/4000")                                                             \
    X(Shutter1_10000, 0x0A, "10", "1/10000")                                                       \
    X(SynchroScan, 0x0B, "11", "Synchro")                                                          \
    X(Elc, 0x0C, "12", "ELC")                                                                      \
    X(Shutter1_24, 0x0D, "13", "1/24")                                                             \
    X(Shutter1_25, 0x0E, "14", "1/25")                                                             \
    X(Shutter1_30, 0x0F, "15", "1/30")

/** OFT и #D2. */
#define CCAM_FOR_EACH_ND(X)                                                                        \
    X(Clear, 0, "0", "Clear")                                                                      \
    X(NdQuarter, 1, "1", "1/4")                                                                    \
    X(NdSixteenth, 2, "2", "1/16")                                                                 \
    X(NdSixtyFourth, 3, "3", "1/64")                                                               \
    X(AutoNd, 8, "8", "Auto")

/** OSC, HE870. Коды с 1. */
#define CCAM_FOR_EACH_SC_COARSE(X)                                                                 \
    X(Deg0, 1, "1", "0°")                                                                          \
    X(Deg90, 2, "2", "90°")                                                                        \
    X(Deg180, 3, "3", "180°")                                                                      \
    X(Deg270, 4, "4", "270°")                                                                      \
    X(Deg45, 5, "5", "45°")                                                                        \
    X(Deg135, 6, "6", "135°")                                                                      \
    X(Deg225, 7, "7", "225°")                                                                      \
    X(Deg315, 8, "8", "315°")

/** ORA / OAR, HE870. */
#define CCAM_FOR_EACH_LIGHT_AREA(X)                                                                \
    X(All, 0, "0", "All")                                                                          \
    X(Center, 1, "1", "Center")                                                                    \
    X(TopCut, 5, "5", "Top Cut")                                                                   \
    X(Bottom, 6, "6", "Bottom")                                                                    \
    X(LeftRight, 7, "7", "Left/Right")

/** Off/On. dec — Oxx / #, hex2 — OSD data. */
#define CCAM_FOR_EACH_SWITCH(X)                                                                    \
    X(Off, 0, "0", "00", "Off")                                                                    \
    X(On, 1, "1", "01", "On")

/** Ответ #O. 0…3 — команды питания; n/f — только ответ (Starting, Off P/T). */
#define CCAM_FOR_EACH_POWER(X)                                                                     \
    X(Off, 0, "0", "Off")                                                                          \
    X(On, 1, "1", "On")                                                                            \
    X(OnWithCameraTx, 2, "2", "On+Cam")                                                           \
    X(OnWithoutCameraTx, 3, "3", "On no Cam")                                                     \
    X(Starting, 14, "n", "Starting")                                                               \
    X(OffPtControl, 15, "f", "Off P/T")

/** OFR. */
#define CCAM_FOR_EACH_FIELD_FRAME(X)                                                               \
    X(Field, 0, "0", "Field")                                                                      \
    X(Frame, 1, "1", "Frame1")                                                                     \
    X(Frame2, 2, "2", "Frame2")

/**
 * DCB — COLOR BAR/CAMERA.
 * PDF: E650/HE и большинство — только 0 Camera / 1 Color Bar (Test/Close нет).
 */
#define CCAM_FOR_EACH_COLOR_BAR(X)                                                                 \
    X(Camera, 0, "0", "Camera")                                                                    \
    X(ColorBar, 1, "1", "Color Bar")

/** ORS: 0 Manual, 1 Auto. */
#define CCAM_FOR_EACH_IRIS_AUTO(X)                                                                 \
    X(Manual, 0, "0", "Manual")                                                                    \
    X(Auto, 1, "1", "Auto")

/** #RT. */
#define CCAM_FOR_EACH_PR_MODE(X)                                                                   \
    X(Normal, 0, "0", "Normal")                                                                    \
    X(Diagonal, 1, "1", "Diagonal")

/** #PST. */
#define CCAM_FOR_EACH_PR_SPEED(X)                                                                  \
    X(Slow, 0, "0", "Slow")                                                                        \
    X(Mid, 1, "1", "Mid")                                                                          \
    X(Fast, 2, "2", "Fast")

/** #LC Data2. */
#define CCAM_FOR_EACH_LIMITATION(X)                                                                \
    X(Release, 0, "0", "Release")                                                                  \
    X(Set, 1, "1", "Set")

/** #L / #LC Data1. */
#define CCAM_FOR_EACH_LIMIT_AXIS(X)                                                                \
    X(TiltUp, 1, "1", "Tilt Up")                                                                   \
    X(TiltDown, 2, "2", "Tilt Down")                                                               \
    X(PanLeft, 3, "3", "Pan Left")                                                                 \
    X(PanRight, 4, "4", "Pan Right")

/** #AGL. */
#define CCAM_FOR_EACH_TILT_RANGE(X)                                                                \
    X(Narrow, 0, "0", "Narrow 190°")                                                               \
    X(Wide, 1, "1", "Wide 300°")

/** #FAN / #FA2. Не путать с OSD Fan (Off/On/Auto). */
#define CCAM_FOR_EACH_FAN(X)                                                                       \
    X(Auto, 0, "0", "Auto")                                                                        \
    X(High, 1, "1", "High")                                                                        \
    X(Mid, 2, "2", "Mid")                                                                          \
    X(Low, 3, "3", "Low")

/** #WIP. */
#define CCAM_FOR_EACH_WIPER(X)                                                                     \
    X(Off, 0, "0", "Off")                                                                          \
    X(Fast, 1, "1", "Fast")                                                                        \
    X(Slow, 2, "2", "Slow")

/** #N Landing. */
#define CCAM_FOR_EACH_LANDING(X)                                                                   \
    X(Just, 0, "0", "Just")                                                                        \
    X(Soft, 1, "1", "Soft")

/** OSD:00. Без enum: шаг 0.01 и FFh Dynamic. */
#define CCAM_FOR_EACH_GAMMA(X)                                                                     \
    X(0x00, "00", "0.35")                                                                          \
    X(0x01, "01", "0.36")                                                                          \
    X(0x02, "02", "0.37")                                                                          \
    X(0x03, "03", "0.38")                                                                          \
    X(0x04, "04", "0.39")                                                                          \
    X(0x05, "05", "0.40")                                                                          \
    X(0x06, "06", "0.41")                                                                          \
    X(0x07, "07", "0.42")                                                                          \
    X(0x08, "08", "0.43")                                                                          \
    X(0x09, "09", "0.44")                                                                          \
    X(0x0A, "0A", "0.45")                                                                          \
    X(0x0B, "0B", "0.46")                                                                          \
    X(0x0C, "0C", "0.47")                                                                          \
    X(0x0D, "0D", "0.48")                                                                          \
    X(0x0E, "0E", "0.49")                                                                          \
    X(0x0F, "0F", "0.50")                                                                          \
    X(0x10, "10", "0.51")                                                                          \
    X(0x11, "11", "0.52")                                                                          \
    X(0x12, "12", "0.53")                                                                          \
    X(0x13, "13", "0.54")                                                                          \
    X(0x14, "14", "0.55")                                                                          \
    X(0xFF, "FF", "Dynamic")

/** OSD:69, студия. */
#define CCAM_FOR_EACH_AGC_STUDIO(X)                                                                \
    X(0x00, "00", "Off")                                                                           \
    X(0x01, "01", "6 dB")                                                                          \
    X(0x02, "02", "12 dB")                                                                         \
    X(0x03, "03", "18 dB")

/** OSD:69, HE. */
#define CCAM_FOR_EACH_AGC_HE(X)                                                                    \
    X(0x00, "00", "Off")                                                                           \
    X(0x01, "01", "6 dB")                                                                          \
    X(0x02, "02", "12 dB")                                                                         \
    X(0x03, "03", "18 dB")                                                                         \
    X(0x04, "04", "24 dB")                                                                         \
    X(0x05, "05", "30 dB")                                                                         \
    X(0x06, "06", "36 dB")                                                                         \
    X(0x07, "07", "42 dB")                                                                         \
    X(0x08, "08", "48 dB")
