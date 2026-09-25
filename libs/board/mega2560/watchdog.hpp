#pragma once

/**
 * Watchdog ATmega2560. После begin() остановить нельзя — нужен периодический kick().
 * Ранний сброс WDT из booloader Mega (секция .init3) сохраняет MCUSR в board_reset_flags.
 */
#include "config.hpp"

#include <avr/wdt.h>
#include <stdint.h>

extern "C" uint8_t board_reset_flags;

struct ResetFlags {
    [[nodiscard]] static uint8_t raw() noexcept { return board_reset_flags; }
    [[nodiscard]] static bool powerOn() noexcept { return (raw() & static_cast<uint8_t>(1u << PORF)) != 0; }
    [[nodiscard]] static bool external() noexcept { return (raw() & static_cast<uint8_t>(1u << EXTRF)) != 0; }
    [[nodiscard]] static bool brownOut() noexcept { return (raw() & static_cast<uint8_t>(1u << BORF)) != 0; }
    [[nodiscard]] static bool watchdog() noexcept { return (raw() & static_cast<uint8_t>(1u << WDRF)) != 0; }
    [[nodiscard]] static bool jtag() noexcept { return (raw() & static_cast<uint8_t>(1u << JTRF)) != 0; }
};

class WatchDog {
public:
    /**
     * Ближайший аппаратный таймаут не короче timeout_ms.
     * Диапазон: 1…8000 мс (ступени 15, 30, 60, 120, 250, 500, 1000, 2000, 4000, 8000).
     */
    [[nodiscard]] bool begin(uint32_t timeout_ms) noexcept
    {
        if (timeout_ms == 0u || timeout_ms > 8000u)
            return false;

        if (timeout_ms <= 15u) {
            wdt_enable(WDTO_15MS);
            _timeoutMs = 15u;
        } else if (timeout_ms <= 30u) {
            wdt_enable(WDTO_30MS);
            _timeoutMs = 30u;
        } else if (timeout_ms <= 60u) {
            wdt_enable(WDTO_60MS);
            _timeoutMs = 60u;
        } else if (timeout_ms <= 120u) {
            wdt_enable(WDTO_120MS);
            _timeoutMs = 120u;
        } else if (timeout_ms <= 250u) {
            wdt_enable(WDTO_250MS);
            _timeoutMs = 250u;
        } else if (timeout_ms <= 500u) {
            wdt_enable(WDTO_500MS);
            _timeoutMs = 500u;
        } else if (timeout_ms <= 1000u) {
            wdt_enable(WDTO_1S);
            _timeoutMs = 1000u;
        } else if (timeout_ms <= 2000u) {
            wdt_enable(WDTO_2S);
            _timeoutMs = 2000u;
        } else if (timeout_ms <= 4000u) {
            wdt_enable(WDTO_4S);
            _timeoutMs = 4000u;
        } else {
            wdt_enable(WDTO_8S);
            _timeoutMs = 8000u;
        }

        _enabled = true;
        return true;
    }

    void kick() noexcept
    {
        if (_enabled)
            wdt_reset();
    }

    [[nodiscard]] bool isEnabled() const noexcept { return _enabled; }
    [[nodiscard]] uint32_t timeoutMs() const noexcept { return _timeoutMs; }

private:
    bool _enabled = false;
    uint32_t _timeoutMs = 0;
};
