#pragma once

/**
 * Векторы ATmega2560. Включать ровно из одного .cpp (src/avr_isr.cpp).
 * До конструкторов гасим watchdog, оставленный бутлоадером Mega, и копируем MCUSR.
 */
#include "clock.hpp"
#include "phl.hpp"
#include "watchdog.hpp"

#include <avr/interrupt.h>
#include <avr/wdt.h>

extern "C" {
uint8_t board_reset_flags __attribute__((section(".noinit")));
}

/*
 * .init3 вклеивается в crt между установкой стека и копированием .data.
 * Это не вызов: обычный epilogue с ret уводит выполнение мимо main.
 * naked — без prologue/epilogue, дальше идёт __do_copy_data.
 * wdt_disable() здесь inline (avr/wdt.h), вызова с ret нет.
 */
extern "C" __attribute__((used, externally_visible, naked, section(".init3")))
void board_wdt_early_off()
{
    board_reset_flags = MCUSR;
    MCUSR = 0;
    wdt_disable();
}

volatile uint32_t BoardClock::ms = 0;

template <>
void (*PHL::Irq<PHL::ID::SERIAL0>::handler)() noexcept = nullptr;
template <>
void (*PHL::Irq<PHL::ID::SERIAL1>::handler)() noexcept = nullptr;
template <>
void (*PHL::Irq<PHL::ID::SERIAL2>::handler)() noexcept = nullptr;
template <>
void (*PHL::Irq<PHL::ID::SERIAL3>::handler)() noexcept = nullptr;

ISR(TIMER0_COMPA_vect)
{
    ++BoardClock::ms;
}

ISR(USART0_RX_vect) { PHL::Irq<PHL::ID::SERIAL0>::invoke(); }
ISR(USART0_UDRE_vect) { PHL::Irq<PHL::ID::SERIAL0>::invoke(); }

ISR(USART1_RX_vect) { PHL::Irq<PHL::ID::SERIAL1>::invoke(); }
ISR(USART1_UDRE_vect) { PHL::Irq<PHL::ID::SERIAL1>::invoke(); }

ISR(USART2_RX_vect) { PHL::Irq<PHL::ID::SERIAL2>::invoke(); }
ISR(USART2_UDRE_vect) { PHL::Irq<PHL::ID::SERIAL2>::invoke(); }

ISR(USART3_RX_vect) { PHL::Irq<PHL::ID::SERIAL3>::invoke(); }
ISR(USART3_UDRE_vect) { PHL::Irq<PHL::ID::SERIAL3>::invoke(); }

/* Виртуальный деструктор ISerial ссылается на sized delete. Куча не используется. */
void operator delete(void*) noexcept {}
void operator delete(void*, unsigned int) noexcept {}
