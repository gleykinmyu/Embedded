#pragma once

#include <cstdint>

#include "impl/serial.hpp"
#include "impl/sd_disk.hpp"
#include "STMboard.h"

class CBoard : public CBaseBoard 
{

public:
    PHL::Serial<PHL::ID::SERIAL1, 2048, 64> serial1;
    PHL::Serial<PHL::ID::SERIAL2, 1024, 128> serial2;
    PHL::SdDisk SD;
};

extern CBoard board;

/** HAL tick (мс) для `nex::Application` (`ClockMsFn`). */
uint32_t boardClockMs() noexcept;

/** Включить/выключить вывод `printf` на serial1 (не блокирует UART при off). */
void setSerial1LogEnabled(bool enabled) noexcept;
