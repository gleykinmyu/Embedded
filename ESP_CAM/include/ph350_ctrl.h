#pragma once

#include "devices/models/ph350.hpp"
#include "rs422_uart.h"
#include "transport/rs485_transport.hpp"
#include "transport/types.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <cstdint>

/** Потокобезопасная обёртка над ccam::devices::Ph350Pt (CLI + HTTP). */
class Ph350Ctrl {
public:
    Ph350Ctrl();
    bool begin();

    ccam::Status power(bool on);
    ccam::Status stop();
    ccam::Status home();
    ccam::Status pan(ccam::PtAxisDir dir, uint8_t rate_1_to_49);
    ccam::Status tilt(ccam::PtAxisDir dir, uint8_t rate_1_to_49);
    ccam::Status panTilt(uint8_t pan_speed, uint8_t tilt_speed);
    ccam::Status panStop();
    ccam::Status tiltStop();
    ccam::Status recallPreset(uint8_t n);
    ccam::Status savePreset(uint8_t n);
    ccam::Status queryPosition(ccam::PtPosition &pos);
    ccam::Status sendRaw(const char *cmd, const char *payload = nullptr);

    uint8_t defaultRate() const { return rate_; }
    void setDefaultRate(uint8_t rate_1_to_49);
    const char *model() const { return "AW-PH350E"; }

    static const char *statusText(ccam::Status st);

private:
    class Lock {
    public:
        explicit Lock(SemaphoreHandle_t m) : m_(m) { xSemaphoreTake(m_, portMAX_DELAY); }
        ~Lock() { xSemaphoreGive(m_); }

    private:
        SemaphoreHandle_t m_;
    };

    static uint32_t nowMs();

    Rs422Uart uart_{};
    ccam::Rs485Transport bus_;
    ccam::devices::Ph350Pt pt_;
    SemaphoreHandle_t mu_{};
    uint8_t rate_{30};
};
