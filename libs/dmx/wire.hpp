/**
 * @file wire.hpp
 * @brief Полудуплекс DMX512 на USART: break ножкой, слоты по прерываниям.
 *
 * Кадр задаёт вызывающий (без start code). Пока isBusy(), он принадлежит
 * прерыванию: приём в него пишет, передача из него читает.
 *
 * Кадр UART 8N2 и 250 кбод включает вызывающий до open() (у потока нет формата).
 * Break и Mark After Break держит сам USART (sendBreak). DE выставляет плата.
 */
#pragma once

#include "dmx_data.hpp"

#include "ibyte_stream.hpp"

#include <cstddef>
#include <cstdint>

namespace dmx::wire {

enum class Direction : uint8_t {
    Receive = 0,
    Transmit = 1,
};

class Transceiver {
public:
    static constexpr uint32_t kBaud = 250000u;
    static constexpr uint16_t kBreakUs = 100u; ///< ≥88 µs, ANSI E1.11
    static constexpr uint16_t kMabUs = 12u;    ///< ≥8 µs Mark After Break

    explicit Transceiver(BIF::IBreakByteStream& uart) noexcept
        : _uart(uart)
    {
    }

    Transceiver(const Transceiver&) = delete;
    Transceiver& operator=(const Transceiver&) = delete;

    /// Кадр без start code. Жив, пока порт им пользуется. nullptr — снять.
    bool bind(Frame* frame) noexcept
    {
        if (isBusy())
            return false;
        _frame = frame;
        return true;
    }

    bool open() noexcept
    {
        if (_open)
            return true;
        _uart.setRxCallback(&Transceiver::rxThunk, this);
        _uart.setTxCallback(&Transceiver::txThunk, this);
        if (!_uart.open(kBaud)) {
            _uart.setRxCallback(nullptr, nullptr);
            _uart.setTxCallback(nullptr, nullptr);
            return false;
        }
        _open = true;
        arm();
        return true;
    }

    void close() noexcept
    {
        if (!_open)
            return;
        _uart.irqRxDisable();
        _uart.irqTxDisable();
        _uart.setRxCallback(nullptr, nullptr);
        _uart.setTxCallback(nullptr, nullptr);
        _uart.close();
        _open = false;
        idle();
    }

    [[nodiscard]] bool isOpen() const noexcept { return _open; }
    [[nodiscard]] Direction direction() const noexcept { return _dir; }

    bool setDirection(Direction dir) noexcept
    {
        if (dir == _dir)
            return true;
        _dir = dir;
        if (_open)
            arm();
        return true;
    }

    /// true, пока линия в кадре. В Transmit после конца кадра сам начинает следующий.
    [[nodiscard]] bool isBusy() noexcept
    {
        if (_phase == Phase::TxDrain && !_uart.isTxBusy())
            _phase = Phase::Idle;
        if (_phase == Phase::Idle && _dir == Direction::Transmit)
            beginTx();
        return _phase != Phase::Idle;
    }

    [[nodiscard]] Status getStatus() const noexcept { return _status; }

    void clearErrors() noexcept { _status = Status::OK; }

    [[nodiscard]] uint32_t frameCount() const noexcept { return _frameCount; }

private:
    enum class Phase : uint8_t {
        Idle,
        RxStart, ///< следующий байт — start code
        RxData,
        TxData,
        TxDrain, ///< слоты ушли в DR, ждём сдвиговый регистр
    };

    static void rxThunk(void* ctx) noexcept
    {
        static_cast<Transceiver*>(ctx)->onRx();
    }

    static void txThunk(void* ctx) noexcept
    {
        static_cast<Transceiver*>(ctx)->onTx();
    }

    void arm() noexcept
    {
        _uart.irqRxDisable();
        _uart.irqTxDisable();
        idle();
        if (_dir == Direction::Receive)
            _uart.irqRxEnable();
        else
            beginTx();
    }

    void beginTx() noexcept
    {
        if (!_open || _dir != Direction::Transmit || _frame == nullptr || _phase != Phase::Idle)
            return;

        _pos = 0u;
        _phase = Phase::TxData;
        _uart.sendBreak(kBreakUs, kMabUs);
        _uart.irqTxEnable();
    }

    void idle() noexcept
    {
        _phase = Phase::Idle;
        _pos = 0u;
    }

    void onRx() noexcept
    {
        if (_dir != Direction::Receive || _frame == nullptr)
            return;

        const BIF::IByteStream::Status st = _uart.getStatus();
        const uint8_t byte = _uart.readByte();
        if (st == BIF::IByteStream::Status::OverFlowRX) {
            _status = Status::OverFlowRX;
            idle();
            return;
        }
        if (st == BIF::IByteStream::Status::DataError) {
            if (_phase == Phase::RxData && _pos > 0u)
                publish();
            _pos = 0u;
            _phase = Phase::RxStart;
            return;
        }
        if (_phase == Phase::RxStart) {
            _phase = (byte == 0u) ? Phase::RxData : Phase::Idle;
            return;
        }
        if (_phase != Phase::RxData)
            return;
        if (_pos < kMaxChannels)
            _frame->channels[_pos++] = byte;
        if (_pos >= kMaxChannels)
            publish();
    }

    void publish() noexcept
    {
        for (std::size_t i = _pos; i < kMaxChannels; ++i)
            _frame->channels[i] = 0;
        idle();
        ++_frameCount;
    }

    void onTx() noexcept
    {
        if (_phase != Phase::TxData || _frame == nullptr) {
            _uart.irqTxDisable();
            return;
        }
        if (_pos == 0u) {
            if (!_uart.writeByte(0u))
                return;
            _pos = 1u;
            return;
        }
        const std::size_t slot = _pos - 1u;
        if (slot >= kMaxChannels) {
            _uart.irqTxDisable();
            _phase = Phase::TxDrain;
            return;
        }
        if (!_uart.writeByte(_frame->channels[slot]))
            return;
        ++_pos;
        if (_pos > kMaxChannels) {
            _uart.irqTxDisable();
            _phase = Phase::TxDrain;
            ++_frameCount;
        }
    }

    BIF::IBreakByteStream& _uart;
    Frame* _frame = nullptr;
    std::size_t _pos = 0u;
    Direction _dir = Direction::Receive;
    volatile Phase _phase = Phase::Idle;
    volatile Status _status = Status::OK;
    volatile uint32_t _frameCount = 0u;
    bool _open = false;
};

} // namespace dmx::wire
