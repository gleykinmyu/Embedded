/**
 * @file transport.hpp
 * @brief Отправка и приём по UART/RS-422/RS-485 через BIF::IByteStream.
 *
 * DE/RE для half-duplex реализуйте внутри своего IByteStream
 * (в write/flush), а не в этом слое.
 * ILockable сериализует send/transact (по умолчанию — пустая заглушка).
 */

#pragma once

#include "ibyte_stream.hpp"
#include "ilockable.hpp"
#include "transport/types.hpp"

namespace ccam {

/** Монотонные миллисекунды (wrap-safe для MsTimer-семантики в transport). */
using NowMsFn = uint32_t (*)();

/** No-op ILockable, если мьютекс не нужен. */
class NullLockable final : public BIF::ILockable {
protected:
    void lock() override {}
    void unlock() override {}
};

inline NullLockable& nullLockable()
{
    static NullLockable instance;
    return instance;
}

/**
 * Транспортный слой поверх IByteStream.
 * Общий для CameraDeviceBase и PtDeviceBase.
 */
class Transport {
public:
    /** Отладочный трейс: dir = 'T' (TX) / 'R' (RX). */
    using TraceFn = void (*)(char dir, const uint8_t* data, size_t len);

    /**
     * @param lock  Критическая секция для send/transact (по умолчанию — nullLockable).
     */
    Transport(BIF::IByteStream& stream, NowMsFn now_ms, BIF::ILockable& lock = nullLockable());

    void setTrace(TraceFn fn) { trace_ = fn; }

    uint32_t nowMs() const { return now_ms_ != nullptr ? now_ms_() : 0; }
    void purge() { stream_.purge(); }

    /** Только передача (без ожидания ACK/ответа). */
    Status send(const uint8_t* data, size_t len);

    /**
     * Передача и ожидание ответа до CR/ETX или timeout.
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
    void emitTrace(char dir, const uint8_t* data, size_t len) const
    {
        if (trace_ != nullptr && data != nullptr && len > 0) {
            trace_(dir, data, len);
        }
    }

    Status sendUnlocked(const uint8_t* data, size_t len);
    Status transactUnlocked(
        const uint8_t* tx,
        size_t tx_len,
        uint8_t* rx,
        size_t rx_cap,
        size_t* rx_len,
        uint32_t timeout_ms);

    BIF::IByteStream& stream_;
    NowMsFn now_ms_;
    BIF::ILockable& lock_;
    TraceFn trace_{nullptr};
};

} // namespace ccam
