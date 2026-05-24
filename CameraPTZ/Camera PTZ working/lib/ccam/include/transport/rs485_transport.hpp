/**
 * @file rs485_transport.hpp
 * @brief Отправка и приём по RS485 через IRs485Hal.
 */

#pragma once

#include "transport/rs485_hal.hpp"
#include "transport/types.hpp"

namespace ccam {

/**
 * Транспортный слой: оборачивает HAL, переключает TX/RX.
 * Общий для CameraDeviceBase и PtDeviceBase.
 */
class Rs485Transport {
public:
    explicit Rs485Transport(IRs485Hal& hal) : hal_(hal) {}

    /** Только передача (большинство P/T-команд без ответа). */
    Status send(const uint8_t* data, size_t len);

    /**
     * Передача (если tx != nullptr) и ожидание ответа.
     * @param rx_len  Фактическая длина принятых данных (может быть nullptr).
     */
    Status transact(
        const uint8_t* tx,
        size_t tx_len,
        uint8_t* rx,
        size_t rx_cap,
        size_t* rx_len,
        uint32_t timeout_ms);

private:
    IRs485Hal& hal_;
};

} // namespace ccam
