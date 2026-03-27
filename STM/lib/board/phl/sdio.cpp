#include "sdio.h"

//============================================================================================================================
//Class function implementation
//============================================================================================================================

CHW_SD::CHW_SD()
{
  _hsd.Instance = SDIO;
  _hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
  _hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
  _hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
  _hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
  _hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
  _hsd.Init.ClockDiv = 0;

}

CHW_Status CHW_SD::Init()
{
    CHW_Status sd_state = HAL_OK;
    /* Check if the SD card is plugged in the slot */
    if (!IsDetected())
    {
        SD_errorMSG("SD isn't present in slot.");
        return HAL_ERROR;
    }
    /* HAL SD initialization */
    sd_state = HAL_SD_Init(&_hsd);
    if (sd_state != HAL_OK) SD_errorMSG("Error HAL_SD_Init.");

    /* Configure SD Bus width (4 bits mode selected) */
    if (sd_state == HAL_OK)
    {
        /* Enable wide operation */
        if (HAL_SD_ConfigWideBusOperation(&_hsd, SDIO_BUS_WIDE_4B) != HAL_OK)
        {
            SD_errorMSG("Error config bus wide 4B.");
            sd_state = HAL_ERROR;
        }
    }

    return sd_state;
}

void CHW_SD::MSP_Init(SD_HandleTypeDef *hsd)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (hsd->Instance == SDIO)
    {
        /* Peripheral clock enable */
        __HAL_RCC_SDIO_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();

        /**SDIO GPIO Configuration
        PC8     ------> SDIO_D0
        PC9     ------> SDIO_D1
        PC10     ------> SDIO_D2
        PC11     ------> SDIO_D3
        PC12     ------> SDIO_CK
        PD2     ------> SDIO_CMD
        */
        GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF12_SDIO;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_2;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF12_SDIO;
        HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

        /* SDIO interrupt Init */
        //HAL_NVIC_SetPriority(SDIO_IRQn, 0, 0);
        //HAL_NVIC_EnableIRQ(SDIO_IRQn);
    }
}

CHW_Status CHW_SD::ConfigWideBus(uint32_t Flag)
{
    return HAL_SD_ConfigWideBusOperation(&_hsd, Flag);
}

CHW_Status CHW_SD::ReadBlocks(uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks, uint32_t Timeout)
{
    CHW_Status sd_state = HAL_SD_ReadBlocks(&_hsd, (uint8_t *)pData, ReadAddr, NumOfBlocks, Timeout);

    if (sd_state != HAL_OK) SD_errorMSG("Error Read Blocks.");
    return sd_state;
}

CHW_Status CHW_SD::WriteBlocks(uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks, uint32_t Timeout)
{
    CHW_Status sd_state = HAL_SD_WriteBlocks(&_hsd, (uint8_t *)pData, WriteAddr, NumOfBlocks, Timeout);

    if (sd_state != HAL_OK) SD_errorMSG("Error Write Blocks.");
    return sd_state;
}

CHW_Status CHW_SD::Erase(uint32_t StartAddr, uint32_t EndAddr)
{
    CHW_Status sd_state = HAL_SD_Erase(&_hsd, StartAddr, EndAddr);

    if (sd_state != HAL_OK) SD_errorMSG("Error Erase SD Card.");
    return sd_state;
}

CHW_SD::State CHW_SD::GetCardState(void)
{
    return HAL_SD_GetCardState(&_hsd);
}

CHW_Status CHW_SD::GetCardInfo(Info *Info)
{
    CHW_Status sd_state = HAL_SD_GetCardInfo(&_hsd, Info);
    if (sd_state != HAL_OK) SD_errorMSG("Error Get Card Info.");
    return sd_state;
}

bool CHW_SD::IsDetected()
{
    return true;
}

//============================================================================================================================
//HAL Library functions
//============================================================================================================================

extern "C" void HAL_SD_MspInit(SD_HandleTypeDef* hsd)
{
    CHW_SD::MSP_Init(hsd);
}

extern "C" void HAL_SD_MspDeInit(SD_HandleTypeDef* hsd)
{
  if(hsd->Instance==SDIO)
  {
    /* Peripheral clock disable */
    __HAL_RCC_SDIO_CLK_DISABLE();
  
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12);
    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_2);

    /* SDIO interrupt DeInit */
    HAL_NVIC_DisableIRQ(SDIO_IRQn);
  }
}
