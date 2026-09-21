/**
 * @file rs485.hpp
 * @brief Half-duplex IDmx: один USART + опциональный DE/RE трансивера.
 */
#pragma once

#include "dmx.hpp"

namespace dmx {

/**
 * Порт RS485: Transmitter и Receiver на одном UART, направление через DE.
 *
 * DE=nullptr — линия всегда в том режиме, как разведена плата (только RX или только TX).
 * DE и RE обычно связаны: high = TX, low = RX.
 */
class Rs485Port : public IDmx {
public:
    Rs485Port(BIF::IHardwareSerial& serial, GPIO::Pin tx, GPIO::AF af, DelayUsFn delayUs,
              const GPIO::Pin* de = nullptr, GPIO::ModeAlt modeAlt = GPIO::ModeAlt::PP) noexcept
        : _tx(serial, tx, af, delayUs, modeAlt)
        , _rx(serial)
        , _de(de)
    {
    }

    bool open() override
    {
        if (_isOpen)
            return true;
        if (_de != nullptr)
            _de->Init(GPIO::Mode::Output, GPIO::Pull::None, GPIO::Speed::VeryHigh);
        applyDe();
        if (!active().open())
            return false;
        _isOpen = true;
        return true;
    }

    void close() override
    {
        _tx.close();
        _rx.close();
        _isOpen = false;
        if (_de != nullptr)
            _de->Write(false);
    }

    [[nodiscard]] bool isOpen() const override { return _isOpen; }
    [[nodiscard]] Transport transport() const override { return Transport::Rs485; }
    [[nodiscard]] Direction direction() const override { return _dir; }

    bool setDirection(Direction dir) override
    {
        if (dir == _dir)
            return true;
        const bool wasOpen = _isOpen;
        if (wasOpen)
            active().close();
        _dir = dir;
        applyDe();
        if (wasOpen && !active().open()) {
            _isOpen = false;
            return false;
        }
        return true;
    }

    [[nodiscard]] uint16_t universe() const override { return _universe; }

    void setUniverse(uint16_t universe) override
    {
        _universe = universe;
        _tx.setUniverse(universe);
        _rx.setUniverse(universe);
    }

    bool send(const Frame& frame) override
    {
        if (!_isOpen || _dir != Direction::Transmit)
            return false;
        return _tx.send(frame);
    }

    bool recv(Frame& frame) override
    {
        if (!_isOpen || _dir != Direction::Receive)
            return false;
        return _rx.recv(frame);
    }

    void poll() override
    {
        if (_isOpen && _dir == Direction::Receive)
            _rx.poll();
    }

    [[nodiscard]] std::size_t available() const override
    {
        return (_isOpen && _dir == Direction::Receive) ? _rx.available() : 0u;
    }

    void purge() override
    {
        if (_dir == Direction::Receive)
            _rx.purge();
    }

    Status getStatus() override
    {
        return _dir == Direction::Receive ? _rx.getStatus() : Status::OK;
    }

    void clearErrors() override
    {
        if (_dir == Direction::Receive)
            _rx.clearErrors();
    }

    [[nodiscard]] uint32_t frameCount() const override
    {
        return _dir == Direction::Transmit ? _tx.frameCount() : _rx.frameCount();
    }

private:
    IDmx& active() noexcept
    {
        return _dir == Direction::Transmit ? static_cast<IDmx&>(_tx) : static_cast<IDmx&>(_rx);
    }

    void applyDe() noexcept
    {
        if (_de != nullptr)
            _de->Write(_dir == Direction::Transmit);
    }

    Transmitter _tx;
    Receiver _rx;
    const GPIO::Pin* _de;
    Direction _dir = Direction::Receive;
    uint16_t _universe = 0;
    bool _isOpen = false;
};

} // namespace dmx
