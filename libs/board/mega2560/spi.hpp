#pragma once

/**
 * SPI master, один модуль ATmega2560.
 * SCK PB1 (D52), MOSI PB2 (D51), MISO PB3 (D50), SS PB0 (D53).
 * SS выводится наружу и держится высоким: низкий вход SS в master сбрасывает бит MSTR.
 * CS устройства — отдельный GPIO.
 */
#include "gpio.hpp"
#include "phl.hpp"

#include <stdint.h>

namespace PHL {

struct SpiMode {
    bool idle_high;
    bool second_edge;
    bool lsb_first;

    static constexpr SpiMode make(bool cpol, bool cpha, bool lsb = false) noexcept
    {
        return {cpol, cpha, lsb};
    }

    static constexpr SpiMode Mode0() noexcept { return make(false, false); }
    static constexpr SpiMode Mode1() noexcept { return make(false, true); }
    static constexpr SpiMode Mode2() noexcept { return make(true, false); }
    static constexpr SpiMode Mode3() noexcept { return make(true, true); }
};

class SpiMaster {
public:
    void InitPins() const noexcept
    {
        GPIO::PortB::pin<0>.Init(GPIO::Mode::Output);
        GPIO::PortB::pin<0>.Set();
        GPIO::PortB::pin<1>.Init(GPIO::Mode::Output);
        GPIO::PortB::pin<2>.Init(GPIO::Mode::Output);
        GPIO::PortB::pin<3>.Init(GPIO::Mode::Input, GPIO::Pull::Up);
    }

    /**
     * Делитель SCK — самый быстрый, при котором F_CPU/div не выше baud_hz.
     * Если ни одна ступень не укладывается — берётся /128.
     * SS (PB0) переводится в выход до установки MSTR.
     */
    [[nodiscard]] bool ConfigureMaster(uint32_t baud_hz, SpiMode mode = SpiMode::Mode0()) noexcept
    {
        if (baud_hz == 0u)
            return false;

        GPIO::PortB::pin<0>.Init(GPIO::Mode::Output);
        GPIO::PortB::pin<0>.Set();

        const Div div = pickDiv(baud_hz);
        uint8_t spcr = static_cast<uint8_t>((1u << SPE) | (1u << MSTR) | div.spr);
        if (mode.second_edge)
            spcr = static_cast<uint8_t>(spcr | (1u << CPHA));
        if (mode.idle_high)
            spcr = static_cast<uint8_t>(spcr | (1u << CPOL));
        if (mode.lsb_first)
            spcr = static_cast<uint8_t>(spcr | (1u << DORD));

        SPSR = div.spi2x ? static_cast<uint8_t>(1u << SPI2X) : 0u;
        SPCR = spcr;
        return true;
    }

    void Shutdown() noexcept { SPCR = static_cast<uint8_t>(SPCR & static_cast<uint8_t>(~(1u << SPE))); }

    [[nodiscard]] uint8_t transfer(uint8_t tx) noexcept
    {
        SPDR = tx;
        while ((SPSR & static_cast<uint8_t>(1u << SPIF)) == 0) {
        }
        return SPDR;
    }

private:
    struct Div {
        uint8_t spr;
        bool spi2x;
        uint16_t div;
    };

    static Div pickDiv(uint32_t baud_hz) noexcept
    {
        constexpr Div table[] = {
            {0u, true, 2u},
            {0u, false, 4u},
            {static_cast<uint8_t>(1u << SPR0), true, 8u},
            {static_cast<uint8_t>(1u << SPR0), false, 16u},
            {static_cast<uint8_t>(1u << SPR1), true, 32u},
            {static_cast<uint8_t>(1u << SPR1), false, 64u},
            {static_cast<uint8_t>((1u << SPR1) | (1u << SPR0)), false, 128u},
        };
        for (const Div& d : table) {
            if ((static_cast<uint32_t>(F_CPU) / d.div) <= baud_hz)
                return d;
        }
        return table[sizeof(table) / sizeof(table[0]) - 1u];
    }
};

} // namespace PHL
