/**
 * @file dmx.hpp
 * @brief DMX512 UART: Transmitter (GPIO Break) и Receiver (sync по FE/DataError).
 *
 * Кадр UART: 8N2 @ 250000 — плата вызывает PHL::Serial::setFrameFormat до open().
 * Оба класса реализуют IDmx; на одном USART одновременно открыт только один.
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

class Transmitter : public IDmx {
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
    [[nodiscard]] Transport transport() const override { return Transport::Rs485; }
    [[nodiscard]] Direction direction() const override { return Direction::Transmit; }

    bool setDirection(Direction dir) override { return dir == Direction::Transmit; }

    [[nodiscard]] uint16_t universe() const override { return _universe; }
    void setUniverse(uint16_t universe) override { _universe = universe; }

    bool send(const Frame& frame) override
    {
        if (!_isOpen || _delayUs == nullptr)
            return false;

        std::size_t n = frame.count;
        if (n > kMaxChannels)
            n = kMaxChannels;

        _serial.flush();

        _tx.Init(GPIO::Mode::Output, GPIO::Pull::None, GPIO::Speed::VeryHigh);
        _tx.Write(false);
        _delayUs(kBreakUs);

        _tx.Init(_modeAlt, _af, GPIO::Pull::Up, GPIO::Speed::VeryHigh);
        _delayUs(kMabUs);

        uint8_t sc = frame.startCode;
        if (!writeAll(&sc, 1u))
            return false;
        if (n > 0u && !writeAll(frame.slots, n))
            return false;

        _serial.flush();
        ++_frameCount;
        return true;
    }

    bool recv(Frame&) override { return false; }
    void poll() override {}
    [[nodiscard]] std::size_t available() const override { return 0u; }
    void purge() override {}

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
    uint16_t _universe = 0;
    uint32_t _frameCount = 0;
    bool _isOpen = false;
};

// ---------------------------------------------------------------------------
// Receiver — sync: Serial FE → sticky DataError (байт Break отбрасывается драйвером)
// ---------------------------------------------------------------------------

class Receiver : public IDmx {
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
    [[nodiscard]] Transport transport() const override { return Transport::Rs485; }
    [[nodiscard]] Direction direction() const override { return Direction::Receive; }

    bool setDirection(Direction dir) override { return dir == Direction::Receive; }

    [[nodiscard]] uint16_t universe() const override { return _universe; }
    void setUniverse(uint16_t universe) override { _universe = universe; }

    bool send(const Frame&) override { return false; }

    bool recv(Frame& frame) override
    {
        if (!_frameReady)
            return false;

        frame.startCode = _frame[0];
        std::size_t channels = (_frameLen > 0u) ? (_frameLen - 1u) : 0u;
        if (channels > kMaxChannels)
            channels = kMaxChannels;

        for (std::size_t i = 0u; i < channels; ++i)
            frame.slots[i] = _frame[1u + i];
        for (std::size_t i = channels; i < kMaxChannels; ++i)
            frame.slots[i] = 0;
        frame.count = static_cast<uint16_t>(channels);

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

        while (_index < kFrameSize) {
            uint8_t b = 0;
            if (_serial.read(&b, 1u) != 1u)
                break;
            _frame[_index++] = b;
        }

        if (_index >= kFrameSize) {
            _frameLen = kFrameSize;
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
        _frameLen = 0u;
        _inFrame = false;
    }

    BIF::IHardwareSerial& _serial;
    uint8_t _frame[kFrameSize]{};
    std::size_t _index = 0u;
    std::size_t _frameLen = 0u;
    uint16_t _universe = 0;
    uint32_t _frameCount = 0;
    Status _status = Status::OK;
    bool _inFrame = false;
    bool _frameReady = false;
    bool _isOpen = false;
};

} // namespace dmx
