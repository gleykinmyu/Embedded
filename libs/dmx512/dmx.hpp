/**
 * @file dmx.hpp
 * @brief DMX512 UART: Rs485Tx (GPIO Break) и Rs485Rx (sync по FE/DataError).
 *
 * Кадр UART: 8N2 @ 250000 — плата вызывает PHL::Serial::setFrameFormat до open().
 * Start code 0 вставляет Tx / снимает Rx; в BIF::dmx::Frame его нет.
 */
#pragma once

#include "idmx.hpp"

#include "core/gpio.h"
#include "iserial.hpp"

namespace dmx {

using Frame = BIF::dmx::Frame;
using Universe = BIF::dmx::Universe;
using Status = BIF::dmx::Status;
using iTx = BIF::dmx::iTx;
using iRx = BIF::dmx::iRx;

using DelayUsFn = void (*)(uint32_t us) noexcept;

inline constexpr uint32_t kBaud = 250000u;
inline constexpr uint16_t kBreakUs = 100u; ///< ≥88 µs по ANSI E1.11
inline constexpr uint16_t kMabUs = 12u;    ///< ≥8 µs Mark After Break
inline constexpr std::size_t kWireFrameSize = 1u + BIF::dmx::kMaxChannels; ///< SC + 512

// ---------------------------------------------------------------------------
// Rs485Tx
// ---------------------------------------------------------------------------

class Rs485Tx : public iTx {
public:
    Rs485Tx(BIF::IHardwareSerial& serial, GPIO::Pin tx, GPIO::AF af, DelayUsFn delayUs,
            GPIO::ModeAlt modeAlt = GPIO::ModeAlt::PP) noexcept
        : _serial(serial)
        , _tx(tx)
        , _af(af)
        , _modeAlt(modeAlt)
        , _delayUs(delayUs)
    {
    }

    bool open() override
    {
        if (_delayUs == nullptr)
            return false;
        if (!_serial.open(kBaud))
            return false;
        _isOpen = true;
        return true;
    }

    void close() override
    {
        _serial.close();
        _isOpen = false;
    }

    [[nodiscard]] bool isOpen() const override { return _isOpen; }

    bool bind(const Universe* uni) override
    {
        _src = uni;
        return true;
    }

    bool unbind(const Universe* uni) override
    {
        if (uni == nullptr || _src != uni)
            return false;
        _src = nullptr;
        return true;
    }

    bool send() override
    {
        if (!_isOpen || _delayUs == nullptr || _src == nullptr)
            return false;

        _serial.flush();

        _tx.Init(GPIO::Mode::Output, GPIO::Pull::None, GPIO::Speed::VeryHigh);
        _tx.Write(false);
        _delayUs(kBreakUs);

        _tx.Init(_modeAlt, _af, GPIO::Pull::Up, GPIO::Speed::VeryHigh);
        _delayUs(kMabUs);

        constexpr uint8_t kStartCode = 0u;
        if (!writeAll(&kStartCode, 1u))
            return false;
        if (!writeAll(_src->data.channels, BIF::dmx::kMaxChannels))
            return false;

        _serial.flush();
        ++_frameCount;
        return true;
    }

    Status getStatus() override { return Status::OK; }
    void clearErrors() override {}
    [[nodiscard]] uint32_t frameCount() const override { return _frameCount; }

private:
    bool writeAll(const uint8_t* data, std::size_t size) noexcept
    {
        std::size_t off = 0u;
        while (off < size) {
            const std::size_t n = _serial.write(data + off, size - off);
            if (n == 0u)
                return false;
            off += n;
        }
        return true;
    }

    BIF::IHardwareSerial& _serial;
    GPIO::Pin _tx;
    GPIO::AF _af;
    GPIO::ModeAlt _modeAlt;
    DelayUsFn _delayUs;
    const Universe* _src = nullptr;
    uint32_t _frameCount = 0;
    bool _isOpen = false;
};

// ---------------------------------------------------------------------------
// Rs485Rx — sync: Serial FE → sticky DataError (байт Break отбрасывается драйвером)
// ---------------------------------------------------------------------------

class Rs485Rx : public iRx {
public:
    explicit Rs485Rx(BIF::IHardwareSerial& serial) noexcept
        : _serial(serial)
    {
    }

    bool open() override
    {
        if (!_serial.open(kBaud))
            return false;
        _isOpen = true;
        resetCollect();
        _frameReady = false;
        return true;
    }

    void close() override
    {
        _serial.close();
        _isOpen = false;
        resetCollect();
        _frameReady = false;
    }

    [[nodiscard]] bool isOpen() const override { return _isOpen; }

    bool bind(Universe* uni) override
    {
        _dst = uni;
        return true;
    }

    bool unbind(Universe* uni) override
    {
        if (uni == nullptr || _dst != uni)
            return false;
        _dst = nullptr;
        return true;
    }

    bool recv() override
    {
        if (!_frameReady || _dst == nullptr)
            return false;

        // _wire[0] = start code (отбрасываем); каналы с 1.
        std::size_t channels = (_wireLen > 0u) ? (_wireLen - 1u) : 0u;
        if (channels > BIF::dmx::kMaxChannels)
            channels = BIF::dmx::kMaxChannels;

        for (std::size_t i = 0u; i < channels; ++i)
            _dst->data.channels[i] = _wire[1u + i];
        for (std::size_t i = channels; i < BIF::dmx::kMaxChannels; ++i)
            _dst->data.channels[i] = 0;

        _frameReady = false;
        return true;
    }

    void poll() override
    {
        if (!_isOpen)
            return;

        if (_serial.getStatus() == BIF::IByteStream::Status::DataError) {
            if (_index > 0u) {
                _wireLen = _index;
                _frameReady = true;
                ++_frameCount;
            }
            _serial.clearErrors();
            _index = 0u;
            _inFrame = true;
        }

        const auto st = _serial.getStatus();
        if (st == BIF::IByteStream::Status::OverFlowRX)
            _status = Status::OverFlowRX;

        if (!_inFrame)
            return;

        while (_index < kWireFrameSize) {
            uint8_t b = 0;
            if (_serial.read(&b, 1u) != 1u)
                break;
            _wire[_index++] = b;
        }

        if (_index >= kWireFrameSize) {
            _wireLen = kWireFrameSize;
            _frameReady = true;
            ++_frameCount;
            _index = 0u;
            _inFrame = false;
        }
    }

    [[nodiscard]] std::size_t available() const override { return _frameReady ? 1u : 0u; }

    void purge() override
    {
        _frameReady = false;
        resetCollect();
        _serial.purge();
    }

    Status getStatus() override { return _status; }

    void clearErrors() override
    {
        _status = Status::OK;
        _serial.clearErrors();
    }

    [[nodiscard]] uint32_t frameCount() const override { return _frameCount; }

private:
    void resetCollect() noexcept
    {
        _index = 0u;
        _wireLen = 0u;
        _inFrame = false;
    }

    BIF::IHardwareSerial& _serial;
    Universe* _dst = nullptr;
    uint8_t _wire[kWireFrameSize]{};
    std::size_t _index = 0u;
    std::size_t _wireLen = 0u;
    uint32_t _frameCount = 0;
    Status _status = Status::OK;
    bool _inFrame = false;
    bool _frameReady = false;
    bool _isOpen = false;
};

// Имена прежней редакции (concrete TX/RX на одном USART).
using Transmitter = Rs485Tx;
using Receiver = Rs485Rx;

} // namespace dmx
