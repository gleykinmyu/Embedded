#pragma once
#include "iserial.hpp"
#include "phl/uart.hpp"
#include "core/critical_section.hpp"

#include <optional>

namespace PHL {

/**
 * Голый USART по PHL::ID. Формат кадра: setFrameFormat при закрытом порте, затем open(baud).
 * RX/TX-прерывания включает клиент (ISerial — приём в open, передачу при записи в кольцо).
 */
template <PHL::ID UartId>
class Uart : public BIF::IBreakByteStream
{
    static_assert(PHL::GetType<UartId>::value == PHL::Type::UART, "Uart: только PHL::ID с Type::UART");

    UART::UART<UartId> _uart;
    UART::Frame _frame = UART::Frame::_8N1();
    std::optional<GPIO::Pin> _txPin;
    GPIO::ModeAlt _txMode = GPIO::ModeAlt::PP;
    GPIO::Pull _txPull = GPIO::Pull::Up;
    uint32_t _primaskSave = 0;
    bool _open = false;

public:
    Uart()
    {
        _uart.IRQ.handler(this, &Uart::uart_irq);
        _uart.IRQ.set_priority(5);
    }

    void InitPins(const GPIO::Pin& tx, const GPIO::Pin& rx,
        GPIO::ModeAlt modeAlt = GPIO::ModeAlt::PP)
    {
        _txMode = modeAlt;
        _txPull = (modeAlt == GPIO::ModeAlt::PP) ? GPIO::Pull::Up : GPIO::Pull::None;
        _txPin.emplace(tx);
        _uart.InitPins(tx, rx, modeAlt);
    }

    ~Uart() override
    {
        if (_open)
            close();
        _uart.IRQ.unregister_handler();
    }

    bool open(uint32_t baud) override
    {
        if (baud == 0U)
            return false;

        _uart.EnableClock();
        if (!_uart.ConfigureAsync(baud, _frame))
            return false;

        _uart.IRQ.enable();
        _open = true;
        return true;
    }

    /// Только при закрытом порте; иначе false. Вступает в силу при следующем open().
    bool setFrameFormat(const UART::Frame& fmt) noexcept
    {
        if (_open)
            return false;
        _frame = fmt;
        return true;
    }

    void close() override
    {
        if (!_open)
            return;
        irqRxDisable();
        irqTxDisable();
        _uart.IRQ.disable();
        _uart.Shutdown();
        _open = false;
    }

    bool isOpen() const override { return _open; }

    void irqRxEnable() override { _uart.cr1.set(UART::CR1::RXNEIE); }
    void irqRxDisable() override { _uart.cr1.clear(UART::CR1::RXNEIE); }
    void irqTxEnable() override { _uart.cr1.set(UART::CR1::TXEIE); }
    void irqTxDisable() override { _uart.cr1.clear(UART::CR1::TXEIE); }

    bool isTxBusy() const override { return !_uart.sr.any(UART::SR::TC); }

    /// TX в GPIO-0 на breakUs, затем снова AF и пауза mabUs. До вызова — InitPins.
    void sendBreak(uint32_t breakUs, uint32_t mabUs = 0) override
    {
        if (!_txPin.has_value() || breakUs == 0u)
            return;
        irqTxDisable();
        while (isTxBusy()) {
        }
        _txPin->Init(GPIO::Mode::Output, GPIO::Pull::None, GPIO::Speed::VeryHigh);
        _txPin->Clear();
        delayUs(breakUs);
        _txPin->Init(_txMode, _uart.af, _txPull, GPIO::Speed::VeryHigh);
        delayUs(mabUs);
    }

protected:
    void lock() override { _primaskSave = CriticalSection::saveAndDisable(); }
    void unlock() override { CriticalSection::restore(_primaskSave); }

private:
    static void delayUs(uint32_t us) noexcept
    {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
        const uint32_t mhz = SystemCoreClock / 1000000u;
        const uint32_t cycles = us * (mhz == 0u ? 1u : mhz);
        const uint32_t start = DWT->CYCCNT;
        while ((DWT->CYCCNT - start) < cycles) {
        }
    }

    void uart_irq() noexcept
    {
        using namespace UART;
        while (_uart.sr.any(SR::RXNE) && _uart.cr1.any(CR1::RXNEIE)) {
            const REG::BitMask<SR> sr = _uart.sr.get();
            BIF::IByteStream::Status st = BIF::IByteStream::Status::OK;
            if (sr.any(SR::ORE))
                st = BIF::IByteStream::Status::OverFlowRX;
            else if (sr.any(SR::FE | SR::NE))
                st = BIF::IByteStream::Status::DataError;
            const uint8_t byte = static_cast<uint8_t>(_uart.dr.read() & 0xFFU);
            _rx.invoke(byte, st);
        }
        if (_uart.sr.any(SR::TXE) && _uart.cr1.any(CR1::TXEIE)) {
            const std::optional<uint8_t> next = _tx.pull();
            if (next.has_value())
                _uart.dr.write(static_cast<uint32_t>(*next));
            else
                irqTxDisable();
        }
    }
};

/**
 * Поток с кольцами поверх Uart. Колбэки — в теле конструктора, когда `_uart` уже жив.
 */
template <PHL::ID UartId, size_t TxSize = 128, size_t RxSize = 128>
class Serial : public BIF::ISerial<TxSize, RxSize>
{
    Uart<UartId> _uart;

public:
    Serial()
        : BIF::ISerial<TxSize, RxSize>(_uart)
    {
        this->bindHw();
    }

    ~Serial() override
    {
        if (this->isOpen())
            this->close();
        this->unbindHw();
    }

    void InitPins(const GPIO::Pin& tx, const GPIO::Pin& rx,
        GPIO::ModeAlt modeAlt = GPIO::ModeAlt::PP)
    {
        _uart.InitPins(tx, rx, modeAlt);
    }

    bool setFrameFormat(const UART::Frame& fmt) noexcept
    {
        return _uart.setFrameFormat(fmt);
    }

    [[nodiscard]] Uart<UartId>& uart() noexcept { return _uart; }
    [[nodiscard]] const Uart<UartId>& uart() const noexcept { return _uart; }
};

} // namespace PHL
