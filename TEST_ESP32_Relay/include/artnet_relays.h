#pragma once

#include "artnet.hpp"
#include "board_pins.h"
#include "lwip_udp.h"
#include "relay_board.h"

class EthW5500;

#ifndef ARTNET_UNIVERSE
#define ARTNET_UNIVERSE 0
#endif
#ifndef ARTNET_START_ADDR
#define ARTNET_START_ADDR 1
#endif
#ifndef ARTNET_THRESHOLD
#define ARTNET_THRESHOLD 128
#endif

/// Art-Net OpDmx → 8 реле. Канал start_addr соответствует Relay 1.
class ArtNetRelays {
public:
    ArtNetRelays(RelayBoard &relays, EthW5500 &eth);

    void begin();

    bool set_universe(uint16_t universe);
    bool set_start(uint16_t addr);
    void set_threshold(uint8_t thr);

    [[nodiscard]] uint16_t universe() const { return cfg_.universe; }
    [[nodiscard]] uint16_t start() const { return start_; }
    [[nodiscard]] uint8_t threshold() const { return threshold_; }
    [[nodiscard]] uint32_t frames() const { return rx_.frameCount(); }
    [[nodiscard]] bool is_open() const { return rx_.isOpen(); }
    void print();

private:
    void loop();
    void apply(const BIF::dmx::Frame &frame);
    static void task(void *arg);

    RelayBoard &relays_;
    EthW5500 &eth_;
    LwipUdp udp_;
    dmx::ArtNet::Config cfg_{};
    dmx::ArtNetRx rx_;
    BIF::dmx::Universe uni_{};
    uint16_t start_{ARTNET_START_ADDR};
    uint8_t threshold_{ARTNET_THRESHOLD};
    uint8_t last_level_[RELAY_COUNT]{};
    bool last_on_[RELAY_COUNT]{};
    bool have_last_{};
};
