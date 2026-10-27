#include "transport/transport.hpp"

namespace ccam {
namespace {

constexpr uint32_t kWriteTimeoutMs = 200;

} // namespace

Transport::Transport(BIF::IByteStream& stream, NowMsFn now_ms, BIF::ILockable& lock)
    : stream_(stream)
    , now_ms_(now_ms)
    , lock_(lock)
{
}

Status Transport::send(const uint8_t* data, size_t len)
{
    BIF::LockGuard guard(lock_);
    return sendUnlocked(data, len);
}

Status Transport::sendUnlocked(const uint8_t* data, size_t len)
{
    if (data == nullptr || len == 0 || now_ms_ == nullptr) {
        return Status::Param;
    }
    if (!stream_.isOpen()) {
        return Status::Io;
    }

    // Старые ответы (eR1 / поздний aPC и т.п.) иначе портят следующий transact.
    stream_.purge();

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
    emitTrace('T', data, len);
    return Status::Ok;
}

Status Transport::transact(
    const uint8_t* tx,
    size_t tx_len,
    uint8_t* rx,
    size_t rx_cap,
    size_t* rx_len,
    uint32_t timeout_ms)
{
    BIF::LockGuard guard(lock_);
    return transactUnlocked(tx, tx_len, rx, rx_cap, rx_len, timeout_ms);
}

Status Transport::transactUnlocked(
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
        const Status st = sendUnlocked(tx, tx_len);
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
        if (n == 0) {
            continue;
        }
        const size_t from = got;
        got += n;
        // P/T кадр заканчивается CR; camera — ETX. Обрываем по любому.
        for (size_t i = from; i < got; ++i) {
            if (rx[i] == kPtCr || rx[i] == kEtx || rx[i] == kAck) {
                got = i + 1;
                if (rx_len != nullptr) {
                    *rx_len = got;
                }
                emitTrace('R', rx, got);
                return Status::Ok;
            }
        }
    }

    if (rx_len != nullptr) {
        *rx_len = got;
    }
    if (got > 0) {
        emitTrace('R', rx, got);
        return Status::Ok;
    }
    return Status::Timeout;
}

} // namespace ccam
