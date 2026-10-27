/**
 * @file types.hpp
 * @brief Общие типы и константы Panasonic Convertible Protocol v3.05.
 *
 * Линия: 9600 8N1 по UART (RS-422 full-duplex или RS-485 half-duplex).
 */

#pragma once

#include "devices/ipt_device.hpp"

#include <cstddef>
#include <cstdint>

namespace ccam {

using Status = pt::Status;

/** Скорость по умолчанию из ConvertibleProtocol.pdf. */
constexpr uint32_t kBaudRate = 9600;
constexpr uint32_t kDefaultRxTimeoutMs = 500;

/* --- Протокол камеры: [STX]…[ETX] --- */
constexpr uint8_t kStx = 0x02;
constexpr uint8_t kEtx = 0x03;
constexpr uint8_t kAck = 0x06;
constexpr char kCmdSep = ':';  ///< Разделитель команды и данных (OAW:1)

/* --- Протокол P/T: #команда[данные]CR --- */
constexpr uint8_t kPtPrefix = '#';
constexpr uint8_t kPtCr = 0x0D;

constexpr size_t kCameraFrameMax = 64;
constexpr size_t kPtFrameMax = 48;

/**
 * Шкала скорости P/T (команды #P, #T, #Z, #F):
 * 01..49 — движение в «отрицательную» сторону (влево / вниз / wide / near),
 * 50     — стоп,
 * 51..99 — движение в «положительную» сторону.
 */
constexpr uint8_t kPtSpeedStop = 50;
constexpr uint8_t kPtSpeedMin = 1;
constexpr uint8_t kPtSpeedMax = 99;

/** Центр абсолютной позиции pan/tilt (#APC, #APS). */
constexpr uint16_t kPtPosCenter = 0x8000;

/**
 * Публичная шкала абсолютной оптики (pt::Lens, setZoom/Focus/Iris, ORV):
 * 0x0000 = min, 0xFFFF = max. Внутри методов → натив протокола.
 */
constexpr uint16_t kNormPosMin = 0x0000;
constexpr uint16_t kNormPosMax = 0xFFFF;

/** Натив P/T #AXZ / #AXF / #AXI / #GZ / #GF / #GI (ConvertibleProtocol). */
constexpr uint16_t kPtLensNativeMin = 0x555;
constexpr uint16_t kPtLensNativeMax = 0xFFF;

/** Натив камеры ORV (Manual Iris volume). */
constexpr uint16_t kCamIrisNativeMin = 0x000;
constexpr uint16_t kCamIrisNativeMax = 0x3FF;

/** norm 0..FFFF → [lo..hi]. */
inline constexpr uint16_t mapNormToRange(uint16_t norm, uint16_t lo, uint16_t hi)
{
    if (hi <= lo) {
        return lo;
    }
    return static_cast<uint16_t>(
        lo + (static_cast<uint32_t>(norm) * static_cast<uint32_t>(hi - lo)) / 0xFFFFu);
}

/** [lo..hi] → norm 0..FFFF. */
inline constexpr uint16_t mapRangeToNorm(uint16_t raw, uint16_t lo, uint16_t hi)
{
    if (hi <= lo) {
        return kNormPosMin;
    }
    if (raw <= lo) {
        return kNormPosMin;
    }
    if (raw >= hi) {
        return kNormPosMax;
    }
    return static_cast<uint16_t>(
        (static_cast<uint32_t>(raw - lo) * 0xFFFFu) / static_cast<uint32_t>(hi - lo));
}

inline constexpr uint16_t normToPtLens(uint16_t norm)
{
    return mapNormToRange(norm, kPtLensNativeMin, kPtLensNativeMax);
}

inline constexpr uint16_t ptLensToNorm(uint16_t raw)
{
    return mapRangeToNorm(raw, kPtLensNativeMin, kPtLensNativeMax);
}

inline constexpr uint16_t normToCamIris(uint16_t norm)
{
    return mapNormToRange(norm, kCamIrisNativeMin, kCamIrisNativeMax);
}

inline constexpr uint16_t camIrisToNorm(uint16_t raw)
{
    return mapRangeToNorm(raw, kCamIrisNativeMin, kCamIrisNativeMax);
}

} // namespace ccam
