#include <Arduino.h>
#include "System.h"

class RelayOutModule
{
public:
    RelayOutModule(HardwareSerial& Serial, uint8_t RelayQt) {_Serial = &Serial; _RelayQt = RelayQt;};


private:
    HardwareSerial* _Serial;
    uint8_t _RelayQt;
    
};


class RelayInputModule
{
public:
    RelayInputModule(/* args */);

private:
    HardwareSerial* _Serial;
};

RelayInputModule::RelayInputModule(/* args */)
{
}

