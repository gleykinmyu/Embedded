#include "artnet_relays.h"

#include <stdio.h>

#include "eth_w5500.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

ArtNetRelays::ArtNetRelays(RelayBoard &relays, EthW5500 &eth)
    : relays_(relays), eth_(eth), rx_(udp_, cfg_) {
    cfg_.universe = ARTNET_UNIVERSE;
    uni_.id = ARTNET_UNIVERSE;
    rx_.setConfig(cfg_);
}

void ArtNetRelays::begin() {
    xTaskCreatePinnedToCore(task, "artnet", 6144, this, 5, nullptr, 1);
}

bool ArtNetRelays::set_universe(uint16_t universe) {
    cfg_.universe = static_cast<uint16_t>(universe & 0x7FFFu);
    uni_.id = cfg_.universe;
    rx_.setConfig(cfg_);
    (void)rx_.bind(&uni_);
    have_last_ = false;
    return true;
}

bool ArtNetRelays::set_start(uint16_t addr) {
    if (addr < 1u || addr > (BIF::dmx::kMaxChannels - RELAY_COUNT + 1u)) {
        return false;
    }
    start_ = addr;
    have_last_ = false;
    return true;
}

void ArtNetRelays::set_threshold(uint8_t thr) {
    threshold_ = thr;
    have_last_ = false;
}

void ArtNetRelays::print() {
    printf("artnet %s uni=%u addr=%u thr=%u dmx=%lu udp=%lu drop=%lu st=%u\n",
           rx_.isOpen() ? "listen" : "down", static_cast<unsigned>(cfg_.universe),
           static_cast<unsigned>(start_), static_cast<unsigned>(threshold_),
           static_cast<unsigned long>(rx_.frameCount()),
           static_cast<unsigned long>(udp_.rx_count()),
           static_cast<unsigned long>(udp_.drop_count()),
           static_cast<unsigned>(rx_.getStatus()));
    if (!have_last_) {
        printf("  no dmx yet\n");
        return;
    }
    printf("  dmx");
    for (size_t i = 0; i < RELAY_COUNT; ++i) {
        printf(" %u:%u%s", static_cast<unsigned>(start_ + i), static_cast<unsigned>(last_level_[i]),
               last_on_[i] ? "*" : "");
    }
    printf("\n");
}

void ArtNetRelays::apply(const BIF::dmx::Frame &frame) {
    for (size_t i = 0; i < RELAY_COUNT; ++i) {
        last_level_[i] = frame.get(static_cast<uint16_t>(start_ + i));
        const bool on = last_level_[i] >= threshold_;
        if (!have_last_ || on != last_on_[i]) {
            (void)relays_.set(i, on);
        }
        last_on_[i] = on;
    }
    have_last_ = true;
}

void ArtNetRelays::loop() {
    while (true) {
        vTaskDelay(1);
        if (!eth_.has_ip()) {
            if (rx_.isOpen()) {
                rx_.close();
            }
            continue;
        }

        if (!rx_.isOpen()) {
            if (!rx_.open()) {
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
            uni_.id = cfg_.universe;
            (void)rx_.bind(&uni_);
        }

        rx_.poll();
        if (rx_.recv()) {
            apply(uni_.data);
        }
    }
}

void ArtNetRelays::task(void *arg) {
    static_cast<ArtNetRelays *>(arg)->loop();
}
