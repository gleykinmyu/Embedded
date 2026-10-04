#include "ph350_ctrl.h"

#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "ph350";

uint32_t Ph350Ctrl::nowMs() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

Ph350Ctrl::Ph350Ctrl() : bus_(uart_, &Ph350Ctrl::nowMs), pt_(bus_) {}

bool Ph350Ctrl::begin() {
    if (mu_ == nullptr) {
        mu_ = xSemaphoreCreateMutex();
        if (mu_ == nullptr) {
            return false;
        }
    }
    if (!uart_.open(ccam::kBaudRate)) {
        return false;
    }
    ESP_LOGI(TAG, "ready model=%s", model());
    return true;
}

void Ph350Ctrl::setDefaultRate(uint8_t rate_1_to_49) {
    if (rate_1_to_49 < 1) {
        rate_1_to_49 = 1;
    }
    if (rate_1_to_49 > 49) {
        rate_1_to_49 = 49;
    }
    rate_ = rate_1_to_49;
}

const char *Ph350Ctrl::statusText(ccam::Status st) {
    switch (st) {
    case ccam::Status::Ok:
        return "ok";
    case ccam::Status::Param:
        return "param";
    case ccam::Status::Io:
        return "io";
    case ccam::Status::Timeout:
        return "timeout";
    case ccam::Status::Nack:
        return "nack";
    case ccam::Status::Buffer:
        return "buffer";
    }
    return "?";
}

ccam::Status Ph350Ctrl::power(bool on) {
    Lock lock(mu_);
    return pt_.power(on ? ccam::PtPowerMode::OnWithCameraTx : ccam::PtPowerMode::Off);
}

ccam::Status Ph350Ctrl::stop() {
    Lock lock(mu_);
    return pt_.stopAll();
}

ccam::Status Ph350Ctrl::home() {
    Lock lock(mu_);
    return pt_.goHome();
}

ccam::Status Ph350Ctrl::pan(ccam::PtAxisDir dir, uint8_t rate_1_to_49) {
    Lock lock(mu_);
    return pt_.movePan(dir, rate_1_to_49);
}

ccam::Status Ph350Ctrl::tilt(ccam::PtAxisDir dir, uint8_t rate_1_to_49) {
    Lock lock(mu_);
    return pt_.moveTilt(dir, rate_1_to_49);
}

ccam::Status Ph350Ctrl::panTilt(uint8_t pan_speed, uint8_t tilt_speed) {
    Lock lock(mu_);
    return pt_.setPanTiltSpeed(pan_speed, tilt_speed);
}

ccam::Status Ph350Ctrl::panStop() {
    Lock lock(mu_);
    return pt_.panStop();
}

ccam::Status Ph350Ctrl::tiltStop() {
    Lock lock(mu_);
    return pt_.tiltStop();
}

ccam::Status Ph350Ctrl::recallPreset(uint8_t n) {
    Lock lock(mu_);
    return pt_.recallPreset(n);
}

ccam::Status Ph350Ctrl::savePreset(uint8_t n) {
    Lock lock(mu_);
    return pt_.savePreset(n);
}

ccam::Status Ph350Ctrl::queryPosition(ccam::PtPosition &pos) {
    Lock lock(mu_);
    return pt_.queryPosition(pos);
}

ccam::Status Ph350Ctrl::sendRaw(const char *cmd, const char *payload) {
    Lock lock(mu_);
    return pt_.sendCommand(cmd, payload);
}
