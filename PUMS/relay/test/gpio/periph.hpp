#pragma once

/**
 * Периферия — шаблон по шине. Пин приходит объектом.
 * const Pin& принимает и Pin, и PinIrq.
 * const PinIrq& принимает только пин с прерыванием.
 */
#include "pin_irq.hpp"

#define GPIO_DECLARE_PORTS
#include "gpio_ports.hpp"
#undef GPIO_DECLARE_PORTS

namespace GPIO {

template <typename Spi>
class W25Q {
public:
    W25Q(Spi& spi, const Pin& cs) noexcept
        : _spi(spi)
        , _cs(cs)
    {
        _cs.Init(Mode::Output);
        _cs.Set();
    }

    void select() const noexcept { _cs.Clear(); }
    void deselect() const noexcept { _cs.Set(); }

private:
    Spi& _spi;
    const Pin& _cs;
};

template <typename Uart>
class SerialPort {
public:
    explicit SerialPort(Uart& uart) noexcept
        : _uart(uart)
    {
    }

    void bindPins(const Pin& tx, const Pin& rx) const noexcept
    {
        tx.Init(Mode::Output);
        rx.Init(Mode::Input);
        (void)_uart;
    }

private:
    Uart& _uart;
};

class Button {
public:
    explicit Button(const PinIrq& pin) noexcept
        : _pin(pin)
    {
        _pin.Init(Mode::Input);
    }

    [[nodiscard]] bool attach(void (*handler)()) const noexcept
    {
        return _pin.attach(Edge::Falling, handler);
    }

    [[nodiscard]] bool Read() const noexcept { return _pin.Read(); }

private:
    const PinIrq& _pin;
};

} // namespace GPIO
