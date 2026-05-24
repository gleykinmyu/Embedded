/**
 * @file rs485_hal.hpp
 * @brief Аппаратный слой: UART + управление DE/RE transceiver.
 *
 * Реализуйте на платформе (STM32 HAL, LL и т.д.) и передайте в Rs485Transport.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace ccam {

/**
 * Интерфейс half-duplex RS485.
 *
 * send()     — передача байт (DE=1 на время отправки).
 * receive()  — приём с таймаутом; вернуть число байт или -1 при ошибке.
 * setTxEnable() — переключение DE/RE (опционально, если transport делает это сам).
 */
class IRs485Hal {
public:
    virtual ~IRs485Hal() = default;

    virtual int send(const uint8_t* data, size_t len) = 0;
    virtual int receive(uint8_t* data, size_t len, uint32_t timeout_ms) = 0;

    virtual void setTxEnable(bool enable) { (void)enable; }
};

} // namespace ccam
