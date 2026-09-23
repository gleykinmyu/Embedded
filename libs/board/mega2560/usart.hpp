#pragma once

/**
 * USART0..USART3 ATmega2560. Биты UCSRnA/B/C совпадают у всех четырёх блоков,
 * поэтому маски взяты из имён USART0 (RXC0, UDRE0, …) — это номера бит Atmel.
 *
 * Пины Mega (фиксированные, без ремапа):
 *   SERIAL0 TX PE1 / RX PE0  (D1 / D0, USB)
 *   SERIAL1 TX PD3 / RX PD2  (D18 / D19)
 *   SERIAL2 TX PH1 / RX PH0  (D16 / D17)
 *   SERIAL3 TX PJ1 / RX PJ0  (D14 / D15)
 */
#include "gpio.hpp"
#include "phl.hpp"

#include <stddef.h>
#include <stdint.h>

namespace UART {

static_assert(RXC0 == RXC1 && RXC1 == RXC2 && RXC2 == RXC3, "UCSRnA.RXC");
static_assert(UDRE0 == UDRE1 && UDRE1 == UDRE2 && UDRE2 == UDRE3, "UCSRnA.UDRE");
static_assert(UDRIE0 == UDRIE1 && UDRIE1 == UDRIE2 && UDRIE2 == UDRIE3, "UCSRnB.UDRIE");
static_assert(RXCIE0 == RXCIE1 && RXEN0 == RXEN1 && TXEN0 == TXEN1, "UCSRnB");
static_assert(UCSZ00 == UCSZ10 && UPM00 == UPM10 && USBS0 == USBS1, "UCSRnC");

enum class DataBits : uint8_t { Bits8 = 8, Bits9 = 9 };

enum class Parity : uint8_t { None, Even, Odd };

enum class StopBits : uint8_t { One = 1, Two = 2 };

/** x8 — U2X (делитель 8), x16 — обычный генератор (делитель 16). */
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
        return make(DataBits::Bits8, Parity::None, StopBits::One, Oversampling::x8);
    }

    static constexpr Frame _8E1() noexcept
    {
        return make(DataBits::Bits8, Parity::Even, StopBits::One, Oversampling::x8);
    }

    static constexpr Frame _8O1() noexcept
    {
        return make(DataBits::Bits8, Parity::Odd, StopBits::One, Oversampling::x8);
    }

    static constexpr Frame _8N2() noexcept
    {
        return make(DataBits::Bits8, Parity::None, StopBits::Two, Oversampling::x8);
    }

    static constexpr Frame _8E2() noexcept
    {
        return make(DataBits::Bits8, Parity::Even, StopBits::Two, Oversampling::x8);
    }

    static constexpr Frame _8O2() noexcept
    {
        return make(DataBits::Bits8, Parity::Odd, StopBits::Two, Oversampling::x8);
    }
};

template <PHL::ID Id>
struct Regs;

template <>
struct Regs<PHL::ID::SERIAL0> {
    static volatile uint8_t& a() noexcept { return UCSR0A; }
    static volatile uint8_t& b() noexcept { return UCSR0B; }
    static volatile uint8_t& c() noexcept { return UCSR0C; }
    static volatile uint8_t& ubrrl() noexcept { return UBRR0L; }
    static volatile uint8_t& ubrrh() noexcept { return UBRR0H; }
    static volatile uint8_t& d() noexcept { return UDR0; }
};

template <>
struct Regs<PHL::ID::SERIAL1> {
    static volatile uint8_t& a() noexcept { return UCSR1A; }
    static volatile uint8_t& b() noexcept { return UCSR1B; }
    static volatile uint8_t& c() noexcept { return UCSR1C; }
    static volatile uint8_t& ubrrl() noexcept { return UBRR1L; }
    static volatile uint8_t& ubrrh() noexcept { return UBRR1H; }
    static volatile uint8_t& d() noexcept { return UDR1; }
};

template <>
struct Regs<PHL::ID::SERIAL2> {
    static volatile uint8_t& a() noexcept { return UCSR2A; }
    static volatile uint8_t& b() noexcept { return UCSR2B; }
    static volatile uint8_t& c() noexcept { return UCSR2C; }
    static volatile uint8_t& ubrrl() noexcept { return UBRR2L; }
    static volatile uint8_t& ubrrh() noexcept { return UBRR2H; }
    static volatile uint8_t& d() noexcept { return UDR2; }
};

template <>
struct Regs<PHL::ID::SERIAL3> {
    static volatile uint8_t& a() noexcept { return UCSR3A; }
    static volatile uint8_t& b() noexcept { return UCSR3B; }
    static volatile uint8_t& c() noexcept { return UCSR3C; }
    static volatile uint8_t& ubrrl() noexcept { return UBRR3L; }
    static volatile uint8_t& ubrrh() noexcept { return UBRR3H; }
    static volatile uint8_t& d() noexcept { return UDR3; }
};

template <PHL::ID Id>
class UART {
    static_assert(PHL::GetType<Id>::value == PHL::Type::UART, "UART: только PHL::ID USART");

    using R = Regs<Id>;

public:
    void InitPins() const noexcept
    {
        if constexpr (Id == PHL::ID::SERIAL0) {
            GPIO::PortE::pin<1>.Init(GPIO::Mode::Output);
            GPIO::PortE::pin<0>.Init(GPIO::Mode::Input, GPIO::Pull::Up);
        } else if constexpr (Id == PHL::ID::SERIAL1) {
            GPIO::PortD::pin<3>.Init(GPIO::Mode::Output);
            GPIO::PortD::pin<2>.Init(GPIO::Mode::Input, GPIO::Pull::Up);
        } else if constexpr (Id == PHL::ID::SERIAL2) {
            GPIO::PortH::pin<1>.Init(GPIO::Mode::Output);
            GPIO::PortH::pin<0>.Init(GPIO::Mode::Input, GPIO::Pull::Up);
        } else if constexpr (Id == PHL::ID::SERIAL3) {
            GPIO::PortJ::pin<1>.Init(GPIO::Mode::Output);
            GPIO::PortJ::pin<0>.Init(GPIO::Mode::Input, GPIO::Pull::Up);
        }
    }

    [[nodiscard]] bool ConfigureAsync(uint32_t baud_hz, const Frame& fmt = Frame::_8N1()) noexcept
    {
        if (baud_hz == 0u || fmt.data_bits != DataBits::Bits8)
            return false;

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
        R::b() = 0;
        uint8_t a = R::a();
        a = static_cast<uint8_t>(a & static_cast<uint8_t>(~((1u << TXC0) | (1u << U2X0))));
        if (u2x)
            a = static_cast<uint8_t>(a | (1u << U2X0));
        R::a() = a;
        R::ubrrh() = static_cast<uint8_t>(ubrr >> 8);
        R::ubrrl() = static_cast<uint8_t>(ubrr);
        R::c() = c;
        R::b() = static_cast<uint8_t>((1u << RXEN0) | (1u << TXEN0));
        return true;
    }

    void Shutdown() noexcept
    {
        CriticalSection cs;
        R::b() = 0;
    }

    [[nodiscard]] uint8_t statusA() const noexcept { return R::a(); }
    [[nodiscard]] uint8_t controlB() const noexcept { return R::b(); }

    [[nodiscard]] bool rxNotEmpty() const noexcept
    {
        return (R::a() & static_cast<uint8_t>(1u << RXC0)) != 0;
    }

    [[nodiscard]] bool dataRegisterEmpty() const noexcept
    {
        return (R::a() & static_cast<uint8_t>(1u << UDRE0)) != 0;
    }

    [[nodiscard]] bool txComplete() const noexcept
    {
        return (R::a() & static_cast<uint8_t>(1u << TXC0)) != 0;
    }

    [[nodiscard]] bool rxIrqEnabled() const noexcept
    {
        return (R::b() & static_cast<uint8_t>(1u << RXCIE0)) != 0;
    }

    [[nodiscard]] bool txIrqEnabled() const noexcept
    {
        return (R::b() & static_cast<uint8_t>(1u << UDRIE0)) != 0;
    }

    void enableRxIrq() noexcept { setBitB(static_cast<uint8_t>(1u << RXCIE0), true); }
    void disableRxIrq() noexcept { setBitB(static_cast<uint8_t>(1u << RXCIE0), false); }
    void enableTxIrq() noexcept { setBitB(static_cast<uint8_t>(1u << UDRIE0), true); }
    void disableTxIrq() noexcept { setBitB(static_cast<uint8_t>(1u << UDRIE0), false); }

    [[nodiscard]] uint8_t readData() noexcept { return R::d(); }
    void writeData(uint8_t data) noexcept { R::d() = data; }

private:
    static void setBitB(uint8_t mask, bool on) noexcept
    {
        CriticalSection cs;
        if (on)
            R::b() = static_cast<uint8_t>(R::b() | mask);
        else
            R::b() = static_cast<uint8_t>(R::b() & static_cast<uint8_t>(~mask));
    }
};

} // namespace UART
