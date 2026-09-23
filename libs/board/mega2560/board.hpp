#pragma once

/**
 * Плата Arduino Mega 2560 без framework Arduino.
 * Векторы Timer0 и USART подключает src/avr_isr.cpp через isr.hpp —
 * этот заголовок сам isr.hpp не включает.
 */
#include "clock.hpp"
#include "led.hpp"
#include "serial.hpp"
#include "spi.hpp"
#include "twi.hpp"
#include "watchdog.hpp"

class CHW_Core {
public:
    CHW_Core() { BoardClock::init(); }

    void Delay(uint32_t time)
    {
        const uint32_t t0 = GetTick();
        while ((GetTick() - t0) < time) {
        }
    }

    uint32_t GetTick() { return BoardClock::millis(); }
};

class CBaseBoard : public CHW_Core {
public:
    CHW_Led led;
};

class CBoard : public CBaseBoard {
public:
    PHL::Serial<PHL::ID::SERIAL0, 128, 64> serial0;
    PHL::Serial<PHL::ID::SERIAL1, 32, 32> serial1;
    PHL::Serial<PHL::ID::SERIAL2, 32, 32> serial2;
    PHL::Serial<PHL::ID::SERIAL3, 32, 32> serial3;
    PHL::SpiMaster spi;
    PHL::TwiMaster twi;
    PHL::WatchDog watchdog;

    /** Kick IWDG, если он запущен; мигание LED раз в 1 с. */
    bool tick() noexcept
    {
        watchdog.kick();
        const uint32_t now = GetTick();
        if ((now - _ledBlinkMs) < 1000u)
            return false;
        _ledBlinkMs = now;
        led.Toggle();
        return true;
    }

private:
    uint32_t _ledBlinkMs = 0;
};

extern CBoard board;
