#pragma once
#include "config.h"
#include "core/irq.h"
#include "core/phl.h"
#include "Debug.h"
//#include "stm32f4xx_hal_sd.h"

#define SD_errorMSG(m) errorMSG(m)

extern "C" void HAL_SD_MspInit(SD_HandleTypeDef* hsd);
extern "C" void SDIO_IRQHandler(void);

class CHW_SD
{
public:

typedef HAL_SD_CardInfoTypeDef Info;
typedef HAL_SD_CardStateTypeDef State;

    CHW_SD();
    CHW_Status Init();
    CHW_Status ConfigWideBus(uint32_t Flag);

    CHW_Status ReadBlocks(uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks, uint32_t Timeout = 0xFFFF);
    CHW_Status WriteBlocks(uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks, uint32_t Timeout = 0xFFFF);

    CHW_Status Erase(uint32_t StartAddr, uint32_t EndAddr);

    State GetCardState(void);
    CHW_Status GetCardInfo(Info *Info);
    bool IsDetected(void);

protected:
    friend void HAL_SD_MspInit(SD_HandleTypeDef* hsd);
    friend void SDIO_IRQHandler(void);
    
    static void MSP_Init(SD_HandleTypeDef* hsd);
    SD_HandleTypeDef _hsd;
};