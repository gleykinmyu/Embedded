/**
 * @file dmx.hpp
 * @brief DMX512 Transmitter (GPIO Break) и Receiver (sync по FE/DataError) поверх IHardwareSerial.
 *
 * Кадр UART: 8N2 @ 250000 — настраивает плата через PHL::Serial::setFrameFormat до open().
 */
#pragma once

#include "idmx.hpp"

#include "core/gpio.h"
#include "iserial.hpp"

namespace dmx {

using DelayUsFn = void (*)(uint32_t us) noexcept;

// ---------------------------------------------------------------------------
// Transmitter
// ---------------------------------------------------------------------------

class Transmitter : public IDmxTransmitter {
public:
    Transmitter(BIF::IHardwareSerial& serial, GPIO::Pin tx, GPIO::AF af, DelayUsFn delayUs,
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

    bool send(uint8_t startCode, const uint8_t* data, std::size_t n) override
    {
        if (!_isOpen || _delayUs == nullptr)
            return false;
        if (n > 0u && data == nullptr)
            return false;
        if (n > kMaxChannels)
            n = kMaxChannels;

        _serial.flush();

        _tx.Init(GPIO::Mode::Output, GPIO::Pull::None, GPIO::Speed::VeryHigh);
        _tx.Write(false);
        _delayUs(kBreakUs);

        _tx.Init(_modeAlt, _af, GPIO::Pull::Up, GPIO::Speed::VeryHigh);
        _delayUs(kMabUs);

        if (!writeAll(&startCode, 1u))
            return false;
        if (n > 0u && !writeAll(data, n))
            return false;

        _serial.flush();
        return true;
    }

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
    bool _isOpen = false;
};

// ---------------------------------------------------------------------------
// Receiver — sync: Serial FE → sticky DataError (байт Break отбрасывается драйвером)
// ---------------------------------------------------------------------------

class Receiver : public IDmxReceiver {
public:
    explicit Receiver(BIF::IHardwareSerial& serial) noexcept
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

    bool receive(uint8_t& startCode, uint8_t* data, std::size_t maxN, std::size_t& n) override
    {
        if (!_frameReady || data == nullptr)
            return false;

        startCode = _frame[0];
        std::size_t channels = (_frameLen > 0u) ? (_frameLen - 1u) : 0u;
        if (channels > maxN)
            channels = maxN;
        if (channels > kMaxChannels)
            channels = kMaxChannels;

        for (std::size_t i = 0u; i < channels; ++i)
            data[i] = _frame[1u + i];

        n = channels;
        _frameReady = false;
        return true;
    }

    void poll() override
    {
        if (!_isOpen)
            return;

        if (_serial.getStatus() == BIF::IByteStream::Status::DataError) {
            if (_index > 0u) {
                _frameLen = _index;
                _frameReady = true;
            }
            _serial.clearErrors();
            _index = 0u;
            _inFrame = true;
        }

        if (!_inFrame)
            return;

        while (_index < kFrameSize) {
            uint8_t b = 0;
            if (_serial.read(&b, 1u) != 1u)
                break;
            _frame[_index++] = b;
        }

        if (_index >= kFrameSize) {
            _frameLen = kFrameSize;
            _frameReady = true;
            _index = 0u;
            _inFrame = false;
        }
    }

private:
    void resetCollect() noexcept
    {
        _index = 0u;
        _frameLen = 0u;
        _inFrame = false;
    }

    BIF::IHardwareSerial& _serial;
    uint8_t _frame[kFrameSize]{};
    std::size_t _index = 0u;
    std::size_t _frameLen = 0u;
    bool _inFrame = false;
    bool _frameReady = false;
    bool _isOpen = false;
};

} // namespace dmx
