/**
 * @file rs485_transport.hpp
 * @brief Отправка и приём по UART/RS-422/RS-485 через BIF::IByteStream.
 *
 * DE/RE для half-duplex реализуйте внутри своего IByteStream
 * (в write/flush), а не в этом слое.
 */

#pragma once

#include "ibyte_stream.hpp"
#include "transport/types.hpp"

namespace ccam {

/** Монотонные миллисекунды (wrap-safe для MsTimer-семантики в transport). */
using NowMsFn = uint32_t (*)();

/**
 * Транспортный слой поверх IByteStream.
 * Общий для CameraDeviceBase и PtDeviceBase.
 */
class Rs485Transport {
public:
    Rs485Transport(BIF::IByteStream& stream, NowMsFn now_ms);

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

    BIF::IByteStream& stream() { return stream_; }

private:
    BIF::IByteStream& stream_;
    NowMsFn now_ms_;
};

} // namespace ccam
