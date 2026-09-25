#pragma once

/**
 * Светодиод Arduino Mega 2560: PB7 (D13), активный высокий уровень.
 */
#include "digital_pin.hpp"

class CHW_Led {
    DigitalPin<Port::B, 7> _pin;

public:
    CHW_Led()
    {
        _pin.Init(BIF::PinMode::Output);
        _pin.Clear();
    }

    void Toggle() { _pin.Toggle(); }
    void On() { _pin.Set(); }
    void Off() { _pin.Clear(); }
};
