/**
 * @file protocol_encoding.hpp
 * @brief Общие значения протокола и кодирование Oxx:n / #Dxx:n (ASCII dec).
 *
 * ConvertibleProtocol.pdf v3.05 — поле data часто одна цифра '0'..'9'.
 */

#pragma once

#include "transport/types.hpp"

#include <cstddef>
#include <cstdint>

namespace ccam {

/** Бинарные Oxx:n / меню 0/1 (ONP, ODE, OUS, OAF, …). */
enum class ProtocolSwitch : uint8_t {
    Off = 0,
    On = 1,
};

/** OFR — Field / Frame. */
enum class FieldFrameMode : uint8_t {
    Field = 0,
    Frame = 1,
};

/** ORS — Iris Auto / Manual. */
enum class IrisAutoManual : uint8_t {
    Auto = 0,
    Manual = 1,
};

/** ODD — 3D-DNR, уровень 0..9. */
enum class Dnr3dLevel : uint8_t {
    Off = 0,
    Level1 = 1,
    Level2 = 2,
    Level3 = 3,
    Level4 = 4,
    Level5 = 5,
    Level6 = 6,
    Level7 = 7,
    Level8 = 8,
    Level9 = 9,
};

/** #D2 ND filter на встроенном PTZ (типичные значения, см. PDF). */
enum class PtNdFilterMode : uint8_t {
    Clear = 0,
    NdQuarter = 1,
    NdSixteenth = 2,
    NdSixtyFourth = 3,
    AutoNd = 8,
};

constexpr const char* protocolSwitchPayload(ProtocolSwitch sw)
{
    return sw == ProtocolSwitch::On ? "1" : "0";
}

inline constexpr ProtocolSwitch toProtocolSwitch(bool on)
{
    return on ? ProtocolSwitch::On : ProtocolSwitch::Off;
}

/** Записать 0..99 в ASCII ('0'..'9' или две цифры). out_cap >= 2. */
inline Status encodeDecAscii(uint8_t value, char* out, size_t out_cap)
{
    if (out == nullptr || out_cap < 2) {
        return Status::Param;
    }

    if (value >= 100) {
        return Status::Param;
    }

    if (value >= 10) {
        if (out_cap < 3) {
            return Status::Param;
        }
        out[0] = static_cast<char>('0' + (value / 10));
        out[1] = static_cast<char>('0' + (value % 10));
        out[2] = '\0';
    } else {
        out[0] = static_cast<char>('0' + value);
        out[1] = '\0';
    }

    return Status::Ok;
}

/** Одна цифра 0..9 (P/T dec1, многие Oxx). */
inline Status encodeDec1(uint8_t value, char* out, size_t out_cap)
{
    if (value > 9) {
        return Status::Param;
    }
    return encodeDecAscii(value, out, out_cap);
}

/** Символ ответа '0'..'9' → число. */
inline Status decodeDec1ResponseByte(uint8_t ascii_byte, uint8_t& value)
{
    if (ascii_byte < '0' || ascii_byte > '9') {
        return Status::Io;
    }
    value = static_cast<uint8_t>(ascii_byte - '0');
    return Status::Ok;
}

} // namespace ccam
