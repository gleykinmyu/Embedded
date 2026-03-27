#pragma once
#include "STMconfig.h"

class CHW_I2C  {
public: 
    CHW_I2C() {
        _handle.Instance = I2C1;
        _handle.Init.ClockSpeed = 100000;
        _handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    }
    
    void Begin(uint32_t Speed = 100000) {
        _handle.Init.ClockSpeed = Speed;
        if (HAL_I2C_Init(&_handle) != HAL_OK) {
            errorMSG("HAL I2C Init not success");
        }
    }

    void Write(uint16_t DevAddr, uint8_t * pData, uint16_t Size, uint32_t Timeout = HAL_MAX_DELAY) {
        HAL_I2C_Master_Transmit(&_handle, DevAddr, pData, Size, Timeout);
    }

private:
    I2C_HandleTypeDef _handle;
};

