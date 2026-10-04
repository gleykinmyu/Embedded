#include "transport/rs485_transport.hpp"

namespace ccam {
namespace {

constexpr uint32_t kWriteTimeoutMs = 200;

} // namespace

Rs485Transport::Rs485Transport(BIF::IByteStream& stream, NowMsFn now_ms)
    : stream_(stream)
    , now_ms_(now_ms)
{
}

Status Rs485Transport::send(const uint8_t* data, size_t len)
{
    if (data == nullptr || len == 0 || now_ms_ == nullptr) {
        return Status::Param;
    }
    if (!stream_.isOpen()) {
        return Status::Io;
    }

    size_t off = 0;
    const uint32_t t0 = now_ms_();
    while (off < len) {
        const size_t n = stream_.write(data + off, len - off);
        if (n > 0) {
            off += n;
            continue;
        }
        if (!stream_.isOpen()) {
            return Status::Io;
        }
        if (static_cast<uint32_t>(now_ms_() - t0) >= kWriteTimeoutMs) {
            return Status::Io;
        }
    }

    stream_.flush();
    return Status::Ok;
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
        const Status st = send(tx, tx_len);
        if (st != Status::Ok) {
            return st;
        }
    }

    if (rx == nullptr || rx_cap == 0) {
        return Status::Ok;
    }
    if (now_ms_ == nullptr) {
        return Status::Param;
    }
    if (!stream_.isOpen()) {
        return Status::Io;
    }

    size_t got = 0;
    const uint32_t t0 = now_ms_();
    while (got < rx_cap) {
        if (static_cast<uint32_t>(now_ms_() - t0) >= timeout_ms) {
            break;
        }
        const size_t n = stream_.read(rx + got, rx_cap - got);
        if (n > 0) {
            got += n;
        }
    }

    if (rx_len != nullptr) {
        *rx_len = got;
    }
    return (got > 0) ? Status::Ok : Status::Timeout;
}

} // namespace ccam
