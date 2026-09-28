/**
 * @file rs485.hpp
 * @brief Half-duplex композиция Rs485Tx + Rs485Rx для тестера (исключение).
 *
 * Пульт/прибор используют Rs485Tx или Rs485Rx напрямую через BIF::dmx::iTx / iRx.
 * DE=nullptr — линия всегда в том режиме, как разведена плата.
 */
#pragma once

#include "dmx.hpp"

namespace dmx {

enum class Role : uint8_t {
    Receive = 0,
    Transmit = 1,
};

/**
 * Порт RS485 для тестера: переключение DE и активной роли TX/RX.
 * Не реализует iTx/iRx сам — отдаёт ссылки на внутренние порты.
 */
class Rs485Port {
public:
    Rs485Port(BIF::IHardwareSerial& serial, GPIO::Pin tx, GPIO::AF af, DelayUsFn delayUs,
              const GPIO::Pin* de = nullptr, GPIO::ModeAlt modeAlt = GPIO::ModeAlt::PP) noexcept
        : _tx(serial, tx, af, delayUs, modeAlt)
        , _rx(serial)
        , _de(de)
    {
    }

    bool open()
    {
        if (_isOpen)
            return true;
        if (_de != nullptr)
            _de->Init(GPIO::Mode::Output, GPIO::Pull::None, GPIO::Speed::VeryHigh);
        applyDe();
        if (!activeOpen())
            return false;
        _isOpen = true;
        return true;
    }

    void close()
    {
        _tx.close();
        _rx.close();
        _isOpen = false;
        if (_de != nullptr)
            _de->Write(false);
    }

    [[nodiscard]] bool isOpen() const { return _isOpen; }
    [[nodiscard]] Role role() const { return _role; }

    bool setRole(Role role)
    {
        if (role == _role)
            return true;
        const bool wasOpen = _isOpen;
        if (wasOpen)
            activeClose();
        _role = role;
        applyDe();
        if (wasOpen && !activeOpen()) {
            _isOpen = false;
            return false;
        }
        return true;
    }

    [[nodiscard]] iTx& tx() noexcept { return _tx; }
    [[nodiscard]] iRx& rx() noexcept { return _rx; }
    [[nodiscard]] const iTx& tx() const noexcept { return _tx; }
    [[nodiscard]] const iRx& rx() const noexcept { return _rx; }

    bool send(const Frame& frame)
    {
        if (!_isOpen || _role != Role::Transmit)
            return false;
        return _tx.send(frame);
    }

    bool recv(Frame& frame)
    {
        if (!_isOpen || _role != Role::Receive)
            return false;
        return _rx.recv(frame);
    }

    void poll()
    {
        if (_isOpen && _role == Role::Receive)
            _rx.poll();
    }

    [[nodiscard]] std::size_t available() const
    {
        return (_isOpen && _role == Role::Receive) ? _rx.available() : 0u;
    }

    void purge()
    {
        if (_role == Role::Receive)
            _rx.purge();
    }

    Status getStatus()
    {
        return _role == Role::Receive ? _rx.getStatus() : Status::OK;
    }

    void clearErrors()
    {
        if (_role == Role::Receive)
            _rx.clearErrors();
    }

    [[nodiscard]] uint32_t frameCount() const
    {
        return _role == Role::Transmit ? _tx.frameCount() : _rx.frameCount();
    }

private:
    bool activeOpen() noexcept
    {
        return _role == Role::Transmit ? _tx.open() : _rx.open();
    }

    void activeClose() noexcept
    {
        if (_role == Role::Transmit)
            _tx.close();
        else
            _rx.close();
    }

    void applyDe() noexcept
    {
        if (_de != nullptr)
            _de->Write(_role == Role::Transmit);
    }

    Rs485Tx _tx;
    Rs485Rx _rx;
    const GPIO::Pin* _de;
    Role _role = Role::Receive;
    bool _isOpen = false;
};

} // namespace dmx
