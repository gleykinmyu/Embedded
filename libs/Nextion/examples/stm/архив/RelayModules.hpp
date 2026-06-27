#pragma once

#include <stm32f4xx_hal.h>

#include "../Interfaces/iserial.hpp"

class RelayOutModule {
public:
    RelayOutModule(BIF::IHardwareSerial& serial, uint8_t RelayQt) : _serial(serial), _RelayQt(RelayQt) {}

    void begin(unsigned baud);
    void RelayProcessing();

    static const uint8_t PortQt = 3;
    uint8_t              ID = 0;

private:
    BIF::IHardwareSerial& _serial;
    uint8_t               _RelayQt;
    unsigned long         _tmr1 = 0;
};

inline void RelayOutModule::begin(unsigned baud)
{
    _serial.open(static_cast<uint32_t>(baud));
    HAL_Delay(100);
    _tmr1 = HAL_GetTick();
    while (_serial.available() > 0) {
        if ((HAL_GetTick() - _tmr1) > 400UL)
            break;
        uint8_t b;
        (void)_serial.read(&b, 1);
    }
}

inline void RelayOutModule::RelayProcessing() {}

class RelayInputModule {
public:
    explicit RelayInputModule(BIF::IHardwareSerial& serial) : _serial(serial) {}

private:
    BIF::IHardwareSerial& _serial;
};
