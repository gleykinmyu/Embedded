#pragma once

/**
 * Тик 1 мс на Timer0 CTC, предделитель 64.
 * TOP = F_CPU/64/1000 - 1. Для 16 МГц Mega это OCR0A = 249.
 * Вектор TIMER0_COMPA_vect — в isr.hpp (один .cpp).
 */
#include "config.hpp"
#include "critical_section.hpp"

#include <avr/io.h>

namespace BoardClock {

extern volatile uint32_t ms;

inline void init() noexcept
{
    constexpr uint32_t counts = static_cast<uint32_t>(F_CPU) / 64u / 1000u;
    static_assert(counts >= 1u && counts <= 256u,
                  "F_CPU/64/1000 не помещается в OCR0A (нужен целый тик 1 мс)");

    ms = 0;
    TCCR0A = static_cast<uint8_t>(1u << WGM01);
    TCCR0B = static_cast<uint8_t>((1u << CS01) | (1u << CS00));
    OCR0A = static_cast<uint8_t>(counts - 1u);
    TCNT0 = 0;
    TIFR0 = static_cast<uint8_t>(1u << OCF0A);
    TIMSK0 = static_cast<uint8_t>(1u << OCIE0A);
    sei();
}

[[nodiscard]] inline uint32_t millis() noexcept
{
    CriticalSection cs;
    return ms;
}

} // namespace BoardClock
