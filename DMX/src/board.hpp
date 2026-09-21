#pragma once

#include <cstddef>
#include <cstdint>

#include "impl/can_bus.hpp"
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
    static constexpr uint32_t kCanBitrate = 1'000'000u;

    PHL::Serial<PHL::ID::SERIAL1, 128, 64> serial1;
    PHL::Serial<PHL::ID::SERIAL2, 5000, 128> serial2;

    /** DMX512 / RS485: USART3 PB10 TX / PB11 RX, DE=PB1. Кадр 8N2, буфер ≥ одного кадра. */
    PHL::Serial<PHL::ID::SERIAL3, 16, 1024> dmx;
    GPIO::Pin dmxDe{GPIO::PortB::pin<1>};
    GPIO::Pin dmxTx{GPIO::PortB::pin<10>};

    /** W25Q16: SCK=PB3, MISO=PB4, MOSI=PB5 (SPI3), CS=PA15 soft. Polling SPI. */
    PHL::SpiStream<PHL::ID::SPI3> flashSpi;
    PHL::W25Q flash{flashSpi, GPIO::PortA::pin<15>};
    PHL::SdDisk SD;
    PHL::Rtc rtc;
    PHL::WatchDog watchdog;

    /** CAN1: RX=PD0, TX=PD1 (AF9). */
    PHL::CanBus<PHL::ID::CAN1, 32, 32> can;

    /** Kick IWDG; при alive — мигание LED раз в 1 с. @return true, если LED переключился. */
    bool tick() noexcept;
    void setLedAlive(bool alive) noexcept;

    /** Пины PD0/PD1 и open(bitrate). */
    bool initCan(uint32_t bitrate = kCanBitrate) noexcept;

    /** USART3 8N2, пины PB10/PB11, DE=PB1 в RX. open() делает Rs485Port. */
    bool initDmxPins() noexcept;

private:
    bool _ledAlive = false;
    uint32_t _ledBlinkMs = 0;
};

extern CBoard board;

/** HAL tick (мс) для `nex::Application` (`ClockMsFn`). */
uint32_t boardClockMs() noexcept;

/** Busy-wait по DWT CYCCNT (Break/MAB DMX). */
void delayUs(uint32_t us) noexcept;

/** Включить/выключить вывод `printf` на serial1 (не блокирует UART при off). */
void setSerial1LogEnabled(bool enabled) noexcept;

/**
 * Доп. приёмник `printf` (тот же поток, что serial1).
 * Вызов из `_write`: только копирование, без UART/Nextion/printf.
 */
using Serial1LogSink = void (*)(const char* data, std::size_t len);
void setSerial1LogSink(Serial1LogSink sink) noexcept;
