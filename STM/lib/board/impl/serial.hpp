#pragma once
#include "iserial.hpp"
#include "phl/uart.hpp"

namespace PHL {

/**
 * Реализация BIF::ISerial для USART/UART из PHL::ID.
 * Формат кадра: setFrameFormat (только при закрытом порте), затем open(baudrate).
 */
template <PHL::ID UartId, size_t TxSize = 256,  size_t RxSize = 256>
class Serial : public BIF::ISerial<TxSize, RxSize>
{
    static_assert(PHL::GetType<UartId>::value == PHL::Type::UART, "Serial: только PHL::ID с Type::UART");

    UART::UART<UartId> _uart;
    UART::Frame _frame = UART::Frame::_8N1();
    uint32_t _primaskSave = 0;

public:
    /// Делегирует в UART::UART (пины TX/RX под альтернативную функцию).
    void InitPins(const GPIO::Pin& tx, const GPIO::Pin& rx) const { _uart.InitPins(tx, rx); }

    ~Serial() { _uart.IRQ.unregister_handler(); }

    bool open(uint32_t baudrate) override
    {
        if (baudrate == 0U)
            return false;

        _uart.EnableClock();

        if (!_uart.ConfigureAsync(baudrate, _frame))
            return false;

        _uart.cr1.set(UART::CR1::RXNEIE);

        _uart.IRQ.handler(this, &Serial::uart_irq);
        _uart.IRQ.set_priority(5);
        _uart.IRQ.enable();

        this->_isOpen = true;
        return true;
    }

    /// Только при закрытом порте; иначе false. Вступает в силу при следующем open().
    bool setFrameFormat(const UART::Frame& fmt) noexcept
    {
        if (this->_isOpen)
            return false;
        _frame = fmt;
        return true;
    }

    void close() override
    {
        _uart.IRQ.disable();
        _uart.IRQ.unregister_handler();

        _uart.cr1.clear(UART::CR1::RXNEIE);
        _uart.cr1.clear(UART::CR1::TXEIE);

        _uart.Shutdown();

        this->_isOpen = false;
    }

    /// Обработчик линии USART IRQ (регистрация: IRQ.handler(this, &Serial::uart_irq)).
    void uart_irq() noexcept
    {
        while (_uart.sr.any(UART::SR::RXNE) && _uart.cr1.any(UART::CR1::RXNEIE)) {
            this->IRQ_RX_Handler();
        }
        if (_uart.sr.any(UART::SR::TXE) && _uart.cr1.any(UART::CR1::TXEIE)) {
            this->IRQ_TX_Handler();
        }
    }

protected:
    void IRQ_TX_Enable() override { _uart.cr1.set(UART::CR1::TXEIE); }

    void IRQ_TX_Disable() override { _uart.cr1.clear(UART::CR1::TXEIE); }

    bool isHardwareTxBusy() const override { return !_uart.sr.any(UART::SR::TC); }

    uint8_t readHardware() override { return static_cast<uint8_t>(_uart.dr.read() & 0xFFU); }

    void writeHardware(uint8_t data) override { _uart.dr.write(static_cast<uint32_t>(data)); }

    bool checkErrors() override
    {
        using namespace UART;

        const REG::BitMask<SR> sr = _uart.sr.get();

        if (sr.any(SR::FE))  this->_isFrameError = true;
        if (sr.any(SR::NE))  this->_isBitError = true;
        if (sr.any(SR::ORE)) this->_isDisconnected = true;
        
        return sr.any(SR::FE | SR::NE);
    }

    void lock() override
    {
        _primaskSave = __get_PRIMASK();
        __disable_irq();
    }

    void unlock() override 
    { 
        __set_PRIMASK(_primaskSave);
    }
};

} // namespace PHL
