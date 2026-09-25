#pragma once

/**
 * BIF::ISerial для USART0..USART3 ATmega2560.
 * Пины фиксированы: 0 — PE1/PE0, 1 — PD3/PD2, 2 — PH1/PH0, 3 — PJ1/PJ0.
 */
#include "critical_section.hpp"
#include "digital_pin.hpp"
#include "iserial.hpp"

#include <avr/io.h>
#include <stddef.h>
#include <stdint.h>

namespace Usart {

inline void (*irq[4])() noexcept = {nullptr, nullptr, nullptr, nullptr};

enum class DataBits : uint8_t { Bits8 = 8 };
enum class Parity : uint8_t { None, Even, Odd };
enum class StopBits : uint8_t { One = 1, Two = 2 };
enum class Oversampling : uint8_t { x16, x8 };

struct Frame {
    DataBits data_bits;
    Parity parity;
    StopBits stop_bits;
    Oversampling oversampling;

    static constexpr Frame make(DataBits d, Parity p, StopBits s,
                                Oversampling o = Oversampling::x8) noexcept
    {
        return {d, p, s, o};
    }

    static constexpr Frame _8N1() noexcept
    {
        return make(DataBits::Bits8, Parity::None, StopBits::One);
    }

    static constexpr Frame _8N2() noexcept
    {
        return make(DataBits::Bits8, Parity::None, StopBits::Two);
    }
};

template <uint8_t N>
struct Regs;

template <>
struct Regs<0> {
    static inline volatile uint8_t& a = UCSR0A;
    static inline volatile uint8_t& b = UCSR0B;
    static inline volatile uint8_t& c = UCSR0C;
    static inline volatile uint8_t& ubrrl = UBRR0L;
    static inline volatile uint8_t& ubrrh = UBRR0H;
    static inline volatile uint8_t& d = UDR0;
};

template <>
struct Regs<1> {
    static inline volatile uint8_t& a = UCSR1A;
    static inline volatile uint8_t& b = UCSR1B;
    static inline volatile uint8_t& c = UCSR1C;
    static inline volatile uint8_t& ubrrl = UBRR1L;
    static inline volatile uint8_t& ubrrh = UBRR1H;
    static inline volatile uint8_t& d = UDR1;
};

template <>
struct Regs<2> {
    static inline volatile uint8_t& a = UCSR2A;
    static inline volatile uint8_t& b = UCSR2B;
    static inline volatile uint8_t& c = UCSR2C;
    static inline volatile uint8_t& ubrrl = UBRR2L;
    static inline volatile uint8_t& ubrrh = UBRR2H;
    static inline volatile uint8_t& d = UDR2;
};

template <>
struct Regs<3> {
    static inline volatile uint8_t& a = UCSR3A;
    static inline volatile uint8_t& b = UCSR3B;
    static inline volatile uint8_t& c = UCSR3C;
    static inline volatile uint8_t& ubrrl = UBRR3L;
    static inline volatile uint8_t& ubrrh = UBRR3H;
    static inline volatile uint8_t& d = UDR3;
};

template <uint8_t N, size_t TxSize = 128, size_t RxSize = 128>
class Serial : public BIF::ISerial<TxSize, RxSize> {
    static_assert(N < 4, "USART 0..3");

    using R = Regs<N>;
    Frame _frame = Frame::_8N1();
    uint8_t _sregSave = 0;
    bool _txArmed = false;
    static inline Serial* self_ = nullptr;

public:
    void InitPins() const
    {
        if constexpr (N == 0) {
            DigitalPin<Port::E, 1>{}.Init(BIF::PinMode::Output);
            DigitalPin<Port::E, 0>{}.Init(BIF::PinMode::Input, BIF::PinPull::Up);
        } else if constexpr (N == 1) {
            DigitalPin<Port::D, 3>{}.Init(BIF::PinMode::Output);
            DigitalPin<Port::D, 2>{}.Init(BIF::PinMode::Input, BIF::PinPull::Up);
        } else if constexpr (N == 2) {
            DigitalPin<Port::H, 1>{}.Init(BIF::PinMode::Output);
            DigitalPin<Port::H, 0>{}.Init(BIF::PinMode::Input, BIF::PinPull::Up);
        } else {
            DigitalPin<Port::J, 1>{}.Init(BIF::PinMode::Output);
            DigitalPin<Port::J, 0>{}.Init(BIF::PinMode::Input, BIF::PinPull::Up);
        }
    }

    ~Serial() override
    {
        if (this->_isOpen)
            close();
        if (self_ == this) {
            irq[N] = nullptr;
            self_ = nullptr;
        }
    }

    bool open(uint32_t baudrate) override
    {
        if (baudrate == 0u || !configure(baudrate, _frame))
            return false;
        self_ = this;
        _txArmed = false;
        irq[N] = &Serial::route;
        setBitB(static_cast<uint8_t>(1u << RXCIE0), true);
        this->clearErrors();
        this->_isOpen = true;
        return true;
    }

    bool setFrameFormat(const Frame& fmt) noexcept
    {
        if (this->_isOpen)
            return false;
        _frame = fmt;
        return true;
    }

    void close() override
    {
        setBitB(static_cast<uint8_t>(1u << RXCIE0), false);
        setBitB(static_cast<uint8_t>(1u << UDRIE0), false);
        irq[N] = nullptr;
        if (self_ == this)
            self_ = nullptr;
        CriticalSection cs;
        R::b = 0;
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
        while ((R::a & static_cast<uint8_t>(1u << RXC0)) != 0 &&
               (R::b & static_cast<uint8_t>(1u << RXCIE0)) != 0)
            this->IRQ_RX_Handler();
        if ((R::a & static_cast<uint8_t>(1u << UDRE0)) != 0 &&
            (R::b & static_cast<uint8_t>(1u << UDRIE0)) != 0)
            this->IRQ_TX_Handler();
    }

    [[nodiscard]] bool configure(uint32_t baud_hz, const Frame& fmt) noexcept
    {
        const bool u2x = fmt.oversampling == Oversampling::x8;
        const uint32_t samples = u2x ? 8u : 16u;
        const uint32_t div = samples * baud_hz;
        const uint32_t rounded = (static_cast<uint32_t>(F_CPU) + div / 2u) / div;
        if (rounded == 0u || rounded > 4096u)
            return false;
        const uint16_t ubrr = static_cast<uint16_t>(rounded - 1u);

        uint8_t c = static_cast<uint8_t>((1u << UCSZ00) | (1u << UCSZ01));
        if (fmt.stop_bits == StopBits::Two)
            c = static_cast<uint8_t>(c | (1u << USBS0));
        if (fmt.parity == Parity::Even)
            c = static_cast<uint8_t>(c | (1u << UPM01));
        else if (fmt.parity == Parity::Odd)
            c = static_cast<uint8_t>(c | (1u << UPM01) | (1u << UPM00));

        CriticalSection cs;
        R::b = 0;
        uint8_t a = R::a;
        a = static_cast<uint8_t>(a & static_cast<uint8_t>(~((1u << TXC0) | (1u << U2X0))));
        if (u2x)
            a = static_cast<uint8_t>(a | (1u << U2X0));
        R::a = a;
        R::ubrrh = static_cast<uint8_t>(ubrr >> 8);
        R::ubrrl = static_cast<uint8_t>(ubrr);
        R::c = c;
        R::b = static_cast<uint8_t>((1u << RXEN0) | (1u << TXEN0));
        return true;
    }

    static void setBitB(uint8_t mask, bool on) noexcept
    {
        CriticalSection cs;
        if (on)
            R::b = static_cast<uint8_t>(R::b | mask);
        else
            R::b = static_cast<uint8_t>(R::b & static_cast<uint8_t>(~mask));
    }

protected:
    void IRQ_TX_Enable() override { setBitB(static_cast<uint8_t>(1u << UDRIE0), true); }
    void IRQ_TX_Disable() override { setBitB(static_cast<uint8_t>(1u << UDRIE0), false); }

    bool isHardwareTxBusy() const override
    {
        if ((R::a & static_cast<uint8_t>(1u << UDRE0)) == 0)
            return true;
        if (!_txArmed)
            return false;
        return (R::a & static_cast<uint8_t>(1u << TXC0)) == 0;
    }

    uint8_t readHardware() override { return R::d; }

    void writeHardware(uint8_t data) override
    {
        _txArmed = true;
        R::d = data;
    }

    bool checkErrors() override
    {
        const uint8_t a = R::a;
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

} // namespace Usart
