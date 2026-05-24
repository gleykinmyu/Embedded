#include "transport/rs485_transport.hpp"

namespace ccam {

Status Rs485Transport::send(const uint8_t* data, size_t len)
{
    if (data == nullptr || len == 0) {
        return Status::Param;
    }

    hal_.setTxEnable(true);
    const int n = hal_.send(data, len);
    hal_.setTxEnable(false);

    return (n == static_cast<int>(len)) ? Status::Ok : Status::Io;
}

Status Rs485Transport::transact(
    const uint8_t* tx,
    size_t tx_len,
    uint8_t* rx,
    size_t rx_cap,
    size_t* rx_len,
    uint32_t timeout_ms)
{
    if (rx_len != nullptr) {
        *rx_len = 0;
    }

    if (tx != nullptr && tx_len > 0) {
        Status st = send(tx, tx_len);
        if (st != Status::Ok) {
            return st;
        }
    }

    if (rx == nullptr || rx_cap == 0) {
        return Status::Ok;
    }

    const int n = hal_.receive(rx, rx_cap, timeout_ms);
    if (n < 0) {
        return Status::Io;
    }
    if (n == 0) {
        return Status::Timeout;
    }

    if (rx_len != nullptr) {
        *rx_len = static_cast<size_t>(n);
    }

    return Status::Ok;
}

} // namespace ccam
