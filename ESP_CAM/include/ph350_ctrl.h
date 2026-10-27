#pragma once

#include "devices/models/e650.hpp"
#include "devices/models/ph350.hpp"
#include "freertos_lock.h"
#include "rs422_uart.h"
#include "transport/transport.hpp"
#include "transport/types.hpp"

#include <cstdint>

/** Плата ETH01: UART RS-422 + lock + Transport + AW-PH350E + AW-E650E. */
class Ph350Ctrl {
public:
    Ph350Ctrl();
    bool begin();

    ccam::devices::Ph350Pt &pt() { return pt_; }
    const ccam::devices::Ph350Pt &pt() const { return pt_; }

    ccam::devices::E650Camera &cam() { return cam_; }
    const ccam::devices::E650Camera &cam() const { return cam_; }

    uint8_t defaultRate() const { return rate_; }
    void setDefaultRate(uint8_t rate_1_to_49);

    static const char *statusText(ccam::Status st);

private:
    static uint32_t nowMs();

    FreertosLock lock_{};
    Rs422Uart uart_{};
    ccam::Transport bus_;
    ccam::devices::Ph350Pt pt_;
    ccam::devices::E650Camera cam_;
    uint8_t rate_{30};
};
