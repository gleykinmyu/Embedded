#pragma once

#include <cstdint>

#include "impl/serial.hpp"
#include "impl/spi_stream.hpp"
#include "impl/w25q.hpp"
#include "impl/sd_disk.hpp"
#include "phl/rtc.hpp"
#include "phl/watchdog.hpp"
#include "STMboard.h"

class CBoard : public CBaseBoard 
{
public:
    PHL::Serial<PHL::ID::SERIAL1, 2048, 64> serial1;
    PHL::Serial<PHL::ID::SERIAL2, 512, 128> serial2;

    /** W25Q16: SCK=PB3, MISO=PB4, MOSI=PB5 (SPI3), CS=PA15 soft. Polling SPI. */
    PHL::SpiStream<PHL::ID::SPI3> flashSpi;
    PHL::W25Q flash{flashSpi, GPIO::PortA::pin<15>};
    PHL::SdDisk SD;
    PHL::Rtc rtc;
    PHL::WatchDog watchdog;

    void tick() noexcept;
    void setLedAlive(bool alive) noexcept;

private:
    bool _ledAlive = false;
    uint32_t _ledBlinkMs = 0;
};

extern CBoard board;

/** HAL tick (мс) для `nex::Application` (`ClockMsFn`). */
uint32_t boardClockMs() noexcept;

/** Включить/выключить вывод `printf` на serial1 (не блокирует UART при off). */
void setSerial1LogEnabled(bool enabled) noexcept;
