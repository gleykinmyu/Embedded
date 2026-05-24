/**
 * @file frame.hpp
 * @brief Буфер для сборки кадров протокола (камера и P/T).
 */

#pragma once

#include "transport/types.hpp"

#include <array>
#include <cstring>

namespace ccam {

/**
 * Фиксированный буфер с пошаговой сборкой ASCII/hex payload.
 * @tparam MaxSize  Максимальная длина кадра в байтах.
 */
template <size_t MaxSize>
class Frame {
public:
    void clear() { len_ = 0; }

    uint8_t* data() { return buffer_.data(); }
    const uint8_t* data() const { return buffer_.data(); }

    size_t size() const { return len_; }
    size_t capacity() const { return MaxSize; }

    Status appendByte(uint8_t value)
    {
        if (len_ >= MaxSize) {
            return Status::Buffer;
        }
        buffer_[len_++] = value;
        return Status::Ok;
    }

    /** Добавить ASCII-строку (команда, payload). */
    Status appendStr(const char* str)
    {
        if (str == nullptr) {
            return Status::Param;
        }
        while (*str != '\0') {
            Status st = appendByte(static_cast<uint8_t>(*str));
            if (st != Status::Ok) {
                return st;
            }
            ++str;
        }
        return Status::Ok;
    }

    /** Две десятичные цифры 01..99 (скорость P/T). */
    Status appendDec2(uint8_t value)
    {
        if (value < 1 || value > 99) {
            return Status::Param;
        }
        if (value >= 10) {
            Status st = appendByte(static_cast<uint8_t>('0' + (value / 10)));
            if (st != Status::Ok) {
                return st;
            }
        } else {
            Status st = appendByte('0');
            if (st != Status::Ok) {
                return st;
            }
        }
        return appendByte(static_cast<uint8_t>('0' + (value % 10)));
    }

    /** Hex ASCII фиксированной ширины (напр. 4 символа для pan в #APC). */
    Status appendHex(uint32_t value, uint8_t width)
    {
        static const char hex[] = "0123456789ABCDEF";

        if (width == 0 || width > 4) {
            return Status::Param;
        }

        for (int i = static_cast<int>(width) - 1; i >= 0; --i) {
            const uint8_t nibble = static_cast<uint8_t>((value >> (i * 4)) & 0x0F);
            Status st = appendByte(static_cast<uint8_t>(hex[nibble]));
            if (st != Status::Ok) {
                return st;
            }
        }
        return Status::Ok;
    }

    /** Два hex-символа для subcode меню (OSD:0A, QSD:48). */
    Status appendHexByte(uint8_t value)
    {
        return appendHex(value, 2);
    }

private:
    std::array<uint8_t, MaxSize> buffer_{};
    size_t len_ = 0;
};

/** Кадр камеры [STX]…[ETX]. */
using CameraFrame = Frame<kCameraFrameMax>;
/** Кадр P/T #…CR. */
using PtFrame = Frame<kPtFrameMax>;

} // namespace ccam
