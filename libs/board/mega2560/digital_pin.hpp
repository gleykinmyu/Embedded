#pragma once

/**
 * IDigitalPin на ATmega. Порт и бит — параметры шаблона.
 * Таблица ниже есть только у этой реализации: слот появляется, если в avr/io.h есть PORTx.
 * Set поднимает бит PORTx, Clear опускает.
 */
#include "idigital_pin.hpp"

#include <avr/io.h>
#include <stdint.h>

enum class Port : uint8_t {
#ifdef PORTA
    A,
#endif
#ifdef PORTB
    B,
#endif
#ifdef PORTC
    C,
#endif
#ifdef PORTD
    D,
#endif
#ifdef PORTE
    E,
#endif
#ifdef PORTF
    F,
#endif
#ifdef PORTG
    G,
#endif
#ifdef PORTH
    H,
#endif
#ifdef PORTJ
    J,
#endif
#ifdef PORTK
    K,
#endif
#ifdef PORTL
    L,
#endif
};

template <Port Id>
struct PortIo;

#ifdef PORTA
template <>
struct PortIo<Port::A> {
    static inline volatile uint8_t& in = PINA;
    static inline volatile uint8_t& dir = DDRA;
    static inline volatile uint8_t& out = PORTA;
};
#endif
#ifdef PORTB
template <>
struct PortIo<Port::B> {
    static inline volatile uint8_t& in = PINB;
    static inline volatile uint8_t& dir = DDRB;
    static inline volatile uint8_t& out = PORTB;
};
#endif
#ifdef PORTC
template <>
struct PortIo<Port::C> {
    static inline volatile uint8_t& in = PINC;
    static inline volatile uint8_t& dir = DDRC;
    static inline volatile uint8_t& out = PORTC;
};
#endif
#ifdef PORTD
template <>
struct PortIo<Port::D> {
    static inline volatile uint8_t& in = PIND;
    static inline volatile uint8_t& dir = DDRD;
    static inline volatile uint8_t& out = PORTD;
};
#endif
#ifdef PORTE
template <>
struct PortIo<Port::E> {
    static inline volatile uint8_t& in = PINE;
    static inline volatile uint8_t& dir = DDRE;
    static inline volatile uint8_t& out = PORTE;
};
#endif
#ifdef PORTF
template <>
struct PortIo<Port::F> {
    static inline volatile uint8_t& in = PINF;
    static inline volatile uint8_t& dir = DDRF;
    static inline volatile uint8_t& out = PORTF;
};
#endif
#ifdef PORTG
template <>
struct PortIo<Port::G> {
    static inline volatile uint8_t& in = PING;
    static inline volatile uint8_t& dir = DDRG;
    static inline volatile uint8_t& out = PORTG;
};
#endif
#ifdef PORTH
template <>
struct PortIo<Port::H> {
    static inline volatile uint8_t& in = PINH;
    static inline volatile uint8_t& dir = DDRH;
    static inline volatile uint8_t& out = PORTH;
};
#endif
#ifdef PORTJ
template <>
struct PortIo<Port::J> {
    static inline volatile uint8_t& in = PINJ;
    static inline volatile uint8_t& dir = DDRJ;
    static inline volatile uint8_t& out = PORTJ;
};
#endif
#ifdef PORTK
template <>
struct PortIo<Port::K> {
    static inline volatile uint8_t& in = PINK;
    static inline volatile uint8_t& dir = DDRK;
    static inline volatile uint8_t& out = PORTK;
};
#endif
#ifdef PORTL
template <>
struct PortIo<Port::L> {
    static inline volatile uint8_t& in = PINL;
    static inline volatile uint8_t& dir = DDRL;
    static inline volatile uint8_t& out = PORTL;
};
#endif

template <Port Id, uint8_t Bit>
class DigitalPin : public BIF::IDigitalPin {
    static_assert(Bit < 8, "пин 0..7");
    static constexpr uint8_t mask = static_cast<uint8_t>(1u << Bit);
    using Io = PortIo<Id>;

public:
    void Init(BIF::PinMode mode, BIF::PinPull pull = BIF::PinPull::None) override
    {
        if (mode == BIF::PinMode::Input) {
            Io::dir &= ~mask;
            if (pull == BIF::PinPull::Up)
                Io::out |= mask;
            else
                Io::out &= ~mask;
            return;
        }
        Io::dir |= mask;
    }

    void Set() override { Io::out |= mask; }

    void Clear() override { Io::out &= ~mask; }

    void Toggle() override { Io::in = mask; }

    [[nodiscard]] bool Read() const override { return (Io::in & mask) != 0; }
};
