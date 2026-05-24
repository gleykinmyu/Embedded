/**
 * @file types.hpp
 * @brief Общие типы и константы Panasonic Convertible Protocol v3.05.
 *
 * Линия: 9600 8N1, half-duplex (RS422/RS485).
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace ccam {

/** Результат операций библиотеки. */
enum class Status {
    Ok = 0,
    Param,    ///< Неверный аргумент
    Io,       ///< Ошибка UART / HAL
    Timeout,  ///< Нет ответа в отведённое время
    Nack,     ///< Камера не вернула ACK (0x06)
    Buffer,   ///< Переполнение буфера кадра
};

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

} // namespace ccam
