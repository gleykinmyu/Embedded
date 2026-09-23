#pragma once

/**
 * Светодиод Arduino Mega 2560: PB7 (D13), активный высокий уровень.
 */
#include "gpio.hpp"

class CHW_Led {
public:
    CHW_Led()
    {
        GPIO::PortB::pin<7>.Init(GPIO::Mode::Output);
        GPIO::PortB::pin<7>.Clear();
    }

    void Toggle() { GPIO::PortB::pin<7>.Toggle(); }
    void On() { GPIO::PortB::pin<7>.Set(); }
    void Off() { GPIO::PortB::pin<7>.Clear(); }
};
