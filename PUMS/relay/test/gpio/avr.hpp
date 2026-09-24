#ifndef GPIO_AVR_HPP
#define GPIO_AVR_HPP

/**
 * Классический AVR: <avr/io.h> открывает iom*.h чипа.
 * В массив попадает порт, только если его макрос есть.
 */
#include <avr/io.h>

namespace GPIO {

using Reg = uint8_t;

struct PortRegs {
    volatile uint8_t* pin;
    volatile uint8_t* ddr;
    volatile uint8_t* port;
};

namespace detail {

#define GPIO_PORT_SLOT(letter) { &PIN##letter, &DDR##letter, &PORT##letter },

inline const PortRegs kPort[] = {
#if defined(PORTA)
    GPIO_PORT_SLOT(A)
#endif
#if defined(PORTB)
    GPIO_PORT_SLOT(B)
#endif
#if defined(PORTC)
    GPIO_PORT_SLOT(C)
#endif
#if defined(PORTD)
    GPIO_PORT_SLOT(D)
#endif
#if defined(PORTE)
    GPIO_PORT_SLOT(E)
#endif
#if defined(PORTF)
    GPIO_PORT_SLOT(F)
#endif
#if defined(PORTG)
    GPIO_PORT_SLOT(G)
#endif
#if defined(PORTH)
    GPIO_PORT_SLOT(H)
#endif
#if defined(PORTJ)
    GPIO_PORT_SLOT(J)
#endif
#if defined(PORTK)
    GPIO_PORT_SLOT(K)
#endif
#if defined(PORTL)
    GPIO_PORT_SLOT(L)
#endif
};

#undef GPIO_PORT_SLOT

} // namespace detail

} // namespace GPIO

#endif

#ifdef GPIO_DECLARE_PORTS
namespace GPIO {

#define GPIO_N0 0
#if defined(PORTA)
using PortA = Port<GPIO_N0, 8>;
#define GPIO_N1 (GPIO_N0 + 1)
#else
#define GPIO_N1 GPIO_N0
#endif
#if defined(PORTB)
using PortB = Port<GPIO_N1, 8>;
#define GPIO_N2 (GPIO_N1 + 1)
#else
#define GPIO_N2 GPIO_N1
#endif
#if defined(PORTC)
using PortC = Port<GPIO_N2, 8>;
#define GPIO_N3 (GPIO_N2 + 1)
#else
#define GPIO_N3 GPIO_N2
#endif
#if defined(PORTD)
#if defined(__AVR_ATmega2560__) || defined(__AVR_ATmega32U4__)
template <> struct PinAt<GPIO_N3, 0> { using type = PinIrq; };
template <> struct PinAt<GPIO_N3, 1> { using type = PinIrq; };
template <> struct PinAt<GPIO_N3, 2> { using type = PinIrq; };
template <> struct PinAt<GPIO_N3, 3> { using type = PinIrq; };
#elif defined(__AVR_ATmega328P__)
template <> struct PinAt<GPIO_N3, 2> { using type = PinIrq; };
template <> struct PinAt<GPIO_N3, 3> { using type = PinIrq; };
#endif
using PortD = Port<GPIO_N3, 8>;
#define GPIO_N4 (GPIO_N3 + 1)
#else
#define GPIO_N4 GPIO_N3
#endif
#if defined(PORTE)
using PortE = Port<GPIO_N4, 8>;
#define GPIO_N5 (GPIO_N4 + 1)
#else
#define GPIO_N5 GPIO_N4
#endif
#if defined(PORTF)
using PortF = Port<GPIO_N5, 8>;
#define GPIO_N6 (GPIO_N5 + 1)
#else
#define GPIO_N6 GPIO_N5
#endif
#if defined(PORTG)
using PortG = Port<GPIO_N6, 8>;
#define GPIO_N7 (GPIO_N6 + 1)
#else
#define GPIO_N7 GPIO_N6
#endif
#if defined(PORTH)
using PortH = Port<GPIO_N7, 8>;
#define GPIO_N8 (GPIO_N7 + 1)
#else
#define GPIO_N8 GPIO_N7
#endif
#if defined(PORTJ)
using PortJ = Port<GPIO_N8, 8>;
#define GPIO_N9 (GPIO_N8 + 1)
#else
#define GPIO_N9 GPIO_N8
#endif
#if defined(PORTK)
using PortK = Port<GPIO_N9, 8>;
#define GPIO_N10 (GPIO_N9 + 1)
#else
#define GPIO_N10 GPIO_N9
#endif
#if defined(PORTL)
using PortL = Port<GPIO_N10, 8>;
#endif

#undef GPIO_N0
#undef GPIO_N1
#undef GPIO_N2
#undef GPIO_N3
#undef GPIO_N4
#undef GPIO_N5
#undef GPIO_N6
#undef GPIO_N7
#undef GPIO_N8
#undef GPIO_N9
#undef GPIO_N10

} // namespace GPIO
#endif
