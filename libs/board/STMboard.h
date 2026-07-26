#pragma once
#include "config.h"
#include "boardLED.h"
#include "core/gpio.h"
#include "core/crash_dump.hpp"
#include "phl/phl.h"

class CHW_Core {
public: 
    CHW_Core() {
        /* Сначала VTOR → RAM (в т.ч. SysTick в слоте 15), затем HAL и тактирование. */
        VectorTableRam::Init();
        HAL_Init();
        SystemClock_Config();
        CrashDump::enableFaultTraps();
    }
    
    void Delay(uint32_t time) { HAL_Delay(time); };
    uint32_t GetTick() { return HAL_GetTick(); };

private: 
    void SystemClock_Config();
};

#undef UART4
#undef UART5
#undef SDIO

class CBaseBoard : public CHW_Core{
public:
    CBaseBoard() : CHW_Core() { };

    CHW_Led led;
};