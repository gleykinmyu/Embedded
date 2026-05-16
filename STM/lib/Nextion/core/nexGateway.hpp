#pragma once
#include <cstdint>
#include "nexProtocol.hpp"
#include "nexMessages.hpp"
#include "nexCommands.hpp"
#include "ibyte_stream.hpp"

namespace nex {

/**
 * **Rx** **Framer** — потоковый разбор UART: накопление байт до полного кадра (заголовок Nextion + полезная нагрузка + `0xFF×3`).
 */
class RxFramer {
public:
    enum class State : uint8_t { WaitHeader, Collect, WaitTerm, Resync };

    RxFrame frame{};

    void reset();
    bool appendByte(uint8_t byte);
    /** Overflow завершён (resync до `0xFF×3`), кадр сброшен — один раз за событие. */
    bool getOverflowReport() noexcept;

private:
    void enterResync(uint8_t byte) noexcept;

    State _state = State::WaitHeader;
    uint8_t _terms = 0;
    bool _overflowReportPending = false;
};

/**
 * **Tx** **Framer** — неблокирующая отправка `TxFrame` в `IByteStream`.
 */
class TxFramer {
public:
    TxFrame frame{};

    void reset() noexcept;
    bool isIdle() const noexcept;
    bool beginRaw(const uint8_t* data, size_t len) noexcept;
    bool tick(BIF::IByteStream& stream) noexcept;

private:
    enum class State : uint8_t { Idle, FramePayload, RawPayload };

    size_t _pos = 0u;
    const uint8_t* _rawData = nullptr;
    size_t _rawLen = 0u;
    State _state = State::Idle;
};

/** Шлюз UART с протоколом Nextion (`pushCommand` / `transmit` / `receive`). */
class Gateway {
public:
    enum class Status : uint8_t {
        OK = 0,
        StreamTxError,
        StreamRxError,
        TxBusy,
        TxBusyRaw,
        RxOverflow,
        SerializeFailed,
        EmptyPayload,
        NullPointer
    };

    explicit Gateway(BIF::IByteStream& s);

    [[nodiscard]] Status getStatus() const noexcept { return _status; }
    void clearError() noexcept { _status = Status::OK; }

    bool isTxIdle() const noexcept { return _txFramer.isIdle(); }

    bool pushCommand(const Command& cmd);
    bool transmit() noexcept;
    bool receive(Message& out);
    bool writeTransparentRaw(const uint8_t* data, size_t len) noexcept;

private:
    BIF::IByteStream& _stream;
    RxFramer _rxFramer;
    TxFramer _txFramer;
    Status _status = Status::OK;
};

inline const char* gatewayStatusCstr(Gateway::Status s) noexcept {
    switch (s) {
    case Gateway::Status::OK: return "OK";
    case Gateway::Status::TxBusy: return "TxBusy";
    case Gateway::Status::SerializeFailed: return "SerializeFailed";
    case Gateway::Status::EmptyPayload: return "EmptyPayload";
    case Gateway::Status::NullPointer: return "NullPointer";
    case Gateway::Status::RxOverflow: return "RxPayloadOverflow";
    case Gateway::Status::TxBusyRaw: return "TxBusyRaw";
    case Gateway::Status::StreamTxError: return "StreamTxError";
    case Gateway::Status::StreamRxError: return "StreamRxError";
    default: return "?";
    }
}

} // namespace nex
