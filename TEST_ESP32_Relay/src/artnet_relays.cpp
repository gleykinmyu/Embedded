#include "artnet_relays.h"

#include <stdio.h>

#include "eth_w5500.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

ArtNetRelays::ArtNetRelays(RelayBoard &relays, EthW5500 &eth)
    : relays_(relays), eth_(eth), node_(udp_) {
    uni_.id = ARTNET_UNIVERSE;
}

namespace {

constexpr char kNvsNs[] = "artnet";
constexpr char kKeyUni[] = "uni";
constexpr char kKeyAddr[] = "addr";
constexpr char kKeyThr[] = "thr";

} // namespace

void ArtNetRelays::begin() {
    load();
    xTaskCreatePinnedToCore(task, "artnet", 6144, this, 5, nullptr, 1);
}

bool ArtNetRelays::set_universe(uint16_t universe) {
    (void)node_.unbind(&uni_);
    uni_.id = static_cast<uint16_t>(universe & 0x7FFFu);
    if (node_.isOpen()) {
        (void)node_.bind(&uni_);
    }
    have_last_ = false;
    save();
    return true;
}

bool ArtNetRelays::set_start(uint16_t addr) {
    if (addr < 1u || addr > (dmx::kMaxChannels - RELAY_COUNT + 1u)) {
        return false;
    }
    start_ = addr;
    have_last_ = false;
    save();
    return true;
}

void ArtNetRelays::set_threshold(uint8_t thr) {
    threshold_ = thr;
    have_last_ = false;
    save();
}

void ArtNetRelays::load() {
    nvs_handle_t h{};
    if (nvs_open(kNvsNs, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    uint16_t uni = 0;
    uint16_t addr = 0;
    uint8_t thr = 0;
    if (nvs_get_u16(h, kKeyUni, &uni) == ESP_OK) {
        uni_.id = static_cast<uint16_t>(uni & 0x7FFFu);
    }
    if (nvs_get_u16(h, kKeyAddr, &addr) == ESP_OK && addr >= 1u &&
        addr <= (dmx::kMaxChannels - RELAY_COUNT + 1u)) {
        start_ = addr;
    }
    if (nvs_get_u8(h, kKeyThr, &thr) == ESP_OK) {
        threshold_ = thr;
    }
    nvs_close(h);
}

void ArtNetRelays::save() {
    nvs_handle_t h{};
    if (nvs_open(kNvsNs, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    (void)nvs_set_u16(h, kKeyUni, uni_.id);
    (void)nvs_set_u16(h, kKeyAddr, start_);
    (void)nvs_set_u8(h, kKeyThr, threshold_);
    (void)nvs_commit(h);
    nvs_close(h);
}

void ArtNetRelays::print() {
    printf("artnet %s uni=%u addr=%u thr=%u dmx=%lu udp=%lu drop=%lu st=%u\n",
           node_.isOpen() ? "listen" : "down", static_cast<unsigned>(uni_.id),
           static_cast<unsigned>(start_), static_cast<unsigned>(threshold_),
           static_cast<unsigned long>(node_.frameCount()),
           static_cast<unsigned long>(udp_.rx_count()),
           static_cast<unsigned long>(udp_.drop_count()),
           static_cast<unsigned>(node_.getStatus()));
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

void ArtNetRelays::apply(const dmx::Frame &frame) {
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
            if (node_.isOpen()) {
                node_.close();
            }
            continue;
        }

        if (!node_.isOpen()) {
            if (!node_.open()) {
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
            (void)node_.bind(&uni_);
        }

        node_.poll();
        if (node_.recv()) {
            apply(uni_.data);
        }
    }
}

void ArtNetRelays::task(void *arg) {
    static_cast<ArtNetRelays *>(arg)->loop();
}
