#pragma once
#include "config.h"
#include "phl/sdio.h"
#include "boardLED.h"
#include "core/gpio.h"
#include "core/phl.h"



class CHW_Core {
public: 
    CHW_Core() {
        HAL_Init();
        SystemClock_Config();  
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

    //Board HW
    CHW_Led led;
    CHW_SD SD;

};