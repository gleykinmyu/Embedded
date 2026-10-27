/**
 * @file protocol_encoding.hpp
 * @brief Кодирование Oxx:n / #Dxx:n (ASCII dec). Типы значений — pt_types.hpp.
 *
 * ConvertibleProtocol.pdf v3.05 — поле data часто одна цифра '0'..'9'.
 */

#pragma once

#include "protocol/pt_types.hpp"
#include "transport/types.hpp"

#include <cstddef>
#include <cstdint>

namespace ccam {

constexpr const char* switchPayload(Switch sw)
{
    return sw == Switch::On ? "1" : "0";
}

inline constexpr Switch toSwitch(bool on)
{
    return on ? Switch::On : Switch::Off;
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
