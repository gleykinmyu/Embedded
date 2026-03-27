#pragma once
#include "config.h"
#include "core/gpio.h"

class CHW_Led {
private: 
    const GPIO::Pin & _pin = GPIO::PortA::pin_inv<1>;
public:
    CHW_Led()
    {
        using namespace GPIO;
       _pin.Init(Mode::Output, Pull::Up);
    }

    inline void Toggle() {
        _pin.Toggle();
    }

    inline void On() {
        _pin.Set();
    }

    inline void Off() {
        _pin.Clear();
    }
};