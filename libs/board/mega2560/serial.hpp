#pragma once

/**
 * BIF::ISerial для USART ATmega2560.
 * Кадр задаётся setFrameFormat() при закрытом порте, затем open(baud).
 * RX — вектор RX, TX — вектор UDRE; оба зовут один uart_irq().
 */
#include "critical_section.hpp"
#include "iserial.hpp"
#include "phl.hpp"
#include "usart.hpp"

namespace PHL {

template <PHL::ID UartId, size_t TxSize = 128, size_t RxSize = 128>
class Serial : public BIF::ISerial<TxSize, RxSize> {
    static_assert(PHL::GetType<UartId>::value == PHL::Type::UART, "Serial: только USART");

    UART::UART<UartId> _uart;
    UART::Frame _frame = UART::Frame::_8N1();
    uint8_t _sregSave = 0;
    bool _txArmed = false;
    static inline Serial* self_ = nullptr;

public:
    void InitPins() const { _uart.InitPins(); }

    ~Serial() override
    {
        if (this->_isOpen)
            close();
        if (self_ == this) {
            Irq<UartId>::set(nullptr);
            self_ = nullptr;
        }
    }

    bool open(uint32_t baudrate) override
    {
        if (baudrate == 0u)
            return false;
        if (!_uart.ConfigureAsync(baudrate, _frame))
            return false;

        self_ = this;
        _txArmed = false;
        Irq<UartId>::set(&Serial::route);
        _uart.enableRxIrq();
        this->clearErrors();
        this->_isOpen = true;
        return true;
    }

    bool setFrameFormat(const UART::Frame& fmt) noexcept
    {
        if (this->_isOpen)
            return false;
        _frame = fmt;
        return true;
    }

    void close() override
    {
        _uart.disableRxIrq();
        _uart.disableTxIrq();
        Irq<UartId>::set(nullptr);
        if (self_ == this)
            self_ = nullptr;
        _uart.Shutdown();
        _txArmed = false;
        this->_isOpen = false;
    }

private:
    static void route() noexcept
    {
        if (self_ != nullptr)
            self_->uart_irq();
    }

    void uart_irq() noexcept
    {
        while (_uart.rxNotEmpty() && _uart.rxIrqEnabled())
            this->IRQ_RX_Handler();
        if (_uart.dataRegisterEmpty() && _uart.txIrqEnabled())
            this->IRQ_TX_Handler();
    }

protected:
    void IRQ_TX_Enable() override { _uart.enableTxIrq(); }
    void IRQ_TX_Disable() override { _uart.disableTxIrq(); }

    bool isHardwareTxBusy() const override
    {
        if (!_uart.dataRegisterEmpty())
            return true;
        if (!_txArmed)
            return false;
        return !_uart.txComplete();
    }

    uint8_t readHardware() override { return _uart.readData(); }

    void writeHardware(uint8_t data) override
    {
        _txArmed = true;
        _uart.writeData(data);
    }

    bool checkErrors() override
    {
        const uint8_t a = _uart.statusA();
        const uint8_t fe = static_cast<uint8_t>(1u << FE0);
        const uint8_t upe = static_cast<uint8_t>(1u << UPE0);
        const uint8_t dor = static_cast<uint8_t>(1u << DOR0);
        if ((a & (fe | upe)) != 0)
            this->_dataError = true;
        if ((a & dor) != 0)
            this->_hwOverrunRx = true;
        return (a & (fe | upe)) != 0;
    }

    void lock() override { _sregSave = CriticalSection::saveAndDisable(); }
    void unlock() override { CriticalSection::restore(_sregSave); }
};

} // namespace PHL
