#pragma once
#include <cstdint>
#include "nexProtocol.hpp"
#include "nexMessages.hpp"
#include "nexCommands.hpp"
#include "nexTimeout.hpp"
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
    enum class Status : uint8_t {
        OK = 0,
        /** `write()==0`, TX-кольцо полно — ждать IRQ (`OverFlowTX`). */
        WaitTxSpace,
        /** Линия недоступна (`IByteStream::Disconnected`). */
        Disconnected,
    };

    TxFrame frame{};

    void reset() noexcept;
    bool isIdle() const noexcept;
    bool beginRaw(const uint8_t* data, size_t len) noexcept;
    Status tick(BIF::IByteStream& stream) noexcept;

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
    /**
     * Последняя ошибка шлюза (`_status`). Выставляет только `Gateway` (nexGateway.cpp);
     * сброс — `clearError()` (успех в `pushCommand` / `writeTransparentRaw` / `receive`,
     * либо снаружи: `Application::clearErrors()`).
     */
    enum class Status : uint8_t {
        /** Нет ошибки; начальное значение и после `clearError()`. */
        OK = 0,
        /** `transmit()`: hard fault потока, stall-timeout TX или `tick()` false. */
        StreamTxError,
        /** `receive()`: сбой RX потока или переполнение RX-кольца — framer reset, `purge()`; при OverFlowRX ещё `clearErrors()`. */
        StreamRxError,
        /** `pushCommand()`: TX занят — предыдущий кадр ещё не ушёл (`!_txFramer.isIdle()`). */
        TxBusy,
        /** `writeTransparentRaw()`: `TxFramer::beginRaw()` не стартовал — TX занят (raw или frame). */
        TxBusyRaw,
        /** `receive()`: `RxFramer` переполнил payload (`MAX_PAYLOAD`), resync до `0xFF×3`; кадр отброшен. */
        RxOverflow,
        /** `pushCommand()`: `Command::serialize()` вернул false; деталь — `cmd.getStatus()`. */
        SerializeFailed,
        /** `pushCommand()` — длина кадра 0 после serialize; `writeTransparentRaw()` — `len == 0`. */
        EmptyPayload,
        /** `writeTransparentRaw()` — `data == nullptr`. */
        NullPointer
    };

    explicit Gateway(BIF::IByteStream& s);

    [[nodiscard]] Status getStatus() const noexcept { return _status; }
    void clearError() noexcept { _status = Status::OK; }

    bool isTxIdle() const noexcept { return _txFramer.isIdle(); }

    bool pushCommand(const Command& cmd);
    /** Продолжить TX; при полном TX-кольце (`write()==0`, `OverFlowTX`) — yield до следующего вызова. */
    bool transmit(uint32_t now_ms, uint32_t timeout_ms) noexcept;
    bool receive(Message& out);
    bool writeTransparentRaw(const uint8_t* data, size_t len) noexcept;

private:
    void recoverStreamRxOverFlow() noexcept;
    void onStreamReadFault(BIF::IByteStream::Status streamSt) noexcept;

    BIF::IByteStream& _stream;
    RxFramer _rxFramer;
    TxFramer _txFramer;
    MsTimer _txStallTimer{};
    Status _status = Status::OK;
};

inline const char* cstr(TxFramer::Status s) noexcept {
    switch (s) {
    case TxFramer::Status::OK: return "OK";
    case TxFramer::Status::WaitTxSpace: return "WaitTxSpace";
    case TxFramer::Status::Disconnected: return "Disconnected";
    default: return "?";
    }
}

inline const char* cstr(Gateway::Status s) noexcept {
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
