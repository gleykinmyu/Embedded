#pragma once

/**
 * GPIO ATmega2560. Пины — порт и бит из даташита Atmel, не номера Arduino.
 * Объект `GPIO::PortB::pin<7>` пустой и constexpr: регистры читаются в момент вызова.
 *
 * Arduino Mega 2560 (для ориентира):
 *   D13 = PB7 (LED), SPI = PB0..PB3, TWI = PD0/PD1,
 *   USART0 = PE0/PE1, USART1 = PD2/PD3, USART2 = PH0/PH1, USART3 = PJ0/PJ1.
 */
#include "config.hpp"
#include "critical_section.hpp"

namespace GPIO {

enum class Mode : uint8_t { Input, Output };

enum class Pull : uint8_t { None, Up };

/** На AVR скорость порта не программируется; аргумент есть, чтобы вызов был знаком по STM-слою. */
enum class Speed : uint8_t { Low, Medium, High, VeryHigh };

class Pin {
public:
    volatile uint8_t* const pin_reg;
    volatile uint8_t* const ddr_reg;
    volatile uint8_t* const port_reg;
    const uint8_t mask;
    const bool inverted;

    Pin(volatile uint8_t* pin, volatile uint8_t* ddr, volatile uint8_t* port,
        uint8_t bit, bool inv) noexcept
        : pin_reg(pin)
        , ddr_reg(ddr)
        , port_reg(port)
        , mask(static_cast<uint8_t>(1u << bit))
        , inverted(inv)
    {
    }

    void Init(Mode mode, Pull pull = Pull::None, Speed speed = Speed::Low) const noexcept
    {
        (void)speed;
        CriticalSection cs;
        if (mode == Mode::Input) {
            *ddr_reg = static_cast<uint8_t>(*ddr_reg & static_cast<uint8_t>(~mask));
            if (pull == Pull::Up)
                *port_reg = static_cast<uint8_t>(*port_reg | mask);
            else
                *port_reg = static_cast<uint8_t>(*port_reg & static_cast<uint8_t>(~mask));
            return;
        }
        *ddr_reg = static_cast<uint8_t>(*ddr_reg | mask);
    }

    void Write(bool level) const noexcept
    {
        const bool phys = inverted ? !level : level;
        CriticalSection cs;
        if (phys)
            *port_reg = static_cast<uint8_t>(*port_reg | mask);
        else
            *port_reg = static_cast<uint8_t>(*port_reg & static_cast<uint8_t>(~mask));
    }

    void Set() const noexcept { Write(true); }
    void Clear() const noexcept { Write(false); }

    /** Запись 1 в PINx переключает защёлку PORTx (одна инструкция). */
    void Toggle() const noexcept { *pin_reg = mask; }

    [[nodiscard]] bool Read() const noexcept
    {
        const bool phys = (*pin_reg & mask) != 0;
        return inverted ? !phys : phys;
    }
};

#define GPIO_PORT(NAME, PINREG, DDRREG, PORTREG)                                   \
    struct Port##NAME {                                                            \
        template <uint8_t Bit, bool Inv>                                           \
        struct Io {                                                                \
            static_assert(Bit < 8, "GPIO: пин 0..7");                              \
            static Pin raw() noexcept                                              \
            {                                                                      \
                return Pin(&(PINREG), &(DDRREG), &(PORTREG), Bit, Inv);            \
            }                                                                      \
            void Init(Mode mode, Pull pull = Pull::None,                           \
                      Speed speed = Speed::Low) const noexcept                     \
            {                                                                      \
                raw().Init(mode, pull, speed);                                     \
            }                                                                      \
            void Write(bool level) const noexcept { raw().Write(level); }          \
            void Set() const noexcept { raw().Set(); }                             \
            void Clear() const noexcept { raw().Clear(); }                         \
            void Toggle() const noexcept { raw().Toggle(); }                       \
            [[nodiscard]] bool Read() const noexcept { return raw().Read(); }      \
        };                                                                         \
        template <uint8_t Bit>                                                     \
        static constexpr Io<Bit, false> pin{};                                     \
        template <uint8_t Bit>                                                     \
        static constexpr Io<Bit, true> pin_inv{};                                  \
    };

GPIO_PORT(A, PINA, DDRA, PORTA)
GPIO_PORT(B, PINB, DDRB, PORTB)
GPIO_PORT(C, PINC, DDRC, PORTC)
GPIO_PORT(D, PIND, DDRD, PORTD)
GPIO_PORT(E, PINE, DDRE, PORTE)
GPIO_PORT(F, PINF, DDRF, PORTF)
GPIO_PORT(G, PING, DDRG, PORTG)
GPIO_PORT(H, PINH, DDRH, PORTH)
GPIO_PORT(J, PINJ, DDRJ, PORTJ)
GPIO_PORT(K, PINK, DDRK, PORTK)
GPIO_PORT(L, PINL, DDRL, PORTL)

#undef GPIO_PORT

} // namespace GPIO
