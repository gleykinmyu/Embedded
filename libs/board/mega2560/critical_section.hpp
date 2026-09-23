/**
 * Критическая секция ATmega: сохранить SREG и cli().
 * RAII: `CriticalSection cs;` — запрет IRQ до конца блока.
 */
#pragma once

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>

class CriticalSection {
    uint8_t _sreg;

public:
    CriticalSection() noexcept
        : _sreg(SREG)
    {
        cli();
    }

    ~CriticalSection() noexcept { SREG = _sreg; }

    CriticalSection(const CriticalSection&) = delete;
    CriticalSection& operator=(const CriticalSection&) = delete;

    [[nodiscard]] static uint8_t saveAndDisable() noexcept
    {
        const uint8_t sreg = SREG;
        cli();
        return sreg;
    }

    static void restore(uint8_t sreg) noexcept { SREG = sreg; }
};
