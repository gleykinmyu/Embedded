#include "ph350_ctrl.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "transport/types.hpp"

#include <cstdio>

static const char *TAG = "ph350";
static const char *TAG_BUS = "ccam";

uint32_t Ph350Ctrl::nowMs() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

namespace {

void format_wire(const uint8_t *data, size_t len, char *out, size_t cap) {
    if (!out || cap == 0) {
        return;
    }
    size_t o = 0;
    for (size_t i = 0; i < len && o + 8 < cap; ++i) {
        const uint8_t b = data[i];
        int n = 0;
        if (b == ccam::kStx) {
            n = snprintf(out + o, cap - o, "%s", "<STX>");
        } else if (b == ccam::kEtx) {
            n = snprintf(out + o, cap - o, "%s", "<ETX>");
        } else if (b == ccam::kAck) {
            n = snprintf(out + o, cap - o, "%s", "<ACK>");
        } else if (b == ccam::kPtCr) {
            n = snprintf(out + o, cap - o, "%s", "<CR>");
        } else if (b == ccam::kPtPrefix) {
            n = snprintf(out + o, cap - o, "%s", "#");
        } else if (b >= 32 && b <= 126) {
            n = snprintf(out + o, cap - o, "%c", static_cast<char>(b));
        } else {
            n = snprintf(out + o, cap - o, "\\x%02X", b);
        }
        if (n <= 0) {
            break;
        }
        o += static_cast<size_t>(n);
    }
    out[o < cap ? o : cap - 1] = '\0';
}

void bus_trace(char dir, const uint8_t *data, size_t len) {
    char text[192];
    format_wire(data, len, text, sizeof(text));
    ESP_LOGI(TAG_BUS, "%cX len=%u %s", dir, static_cast<unsigned>(len), text);
}

} // namespace

Ph350Ctrl::Ph350Ctrl() : bus_(uart_, &Ph350Ctrl::nowMs, lock_), pt_(bus_), cam_(bus_) {}

bool Ph350Ctrl::begin() {
    if (!lock_.ok()) {
        return false;
    }
    if (!uart_.open(ccam::kBaudRate)) {
        return false;
    }
    bus_.setTrace(&bus_trace);
    ESP_LOGI(TAG, "ready pt=%s cam=%s", pt_.modelLabel(), cam_.modelLabel());
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
