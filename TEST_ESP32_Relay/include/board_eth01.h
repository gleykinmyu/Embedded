#pragma once

#include <stdint.h>

// ESP32-ETH01: LAN8720, тактовая RMII 50 МГц приходит на GPIO0.
// Питание осциллятора PHY: GPIO16. На части ревизий это GPIO17.
static constexpr int PIN_ETH_PHY_POWER = 16;
static constexpr int PIN_ETH_MDC = 23;
static constexpr int PIN_ETH_MDIO = 18;
static constexpr int ETH_PHY_ADDR = 1;

#define ETH01_IP4_ADDR  2, 0, 0, 12
#define ETH01_GW4_ADDR  192, 168, 5, 1
#define ETH01_MASK_ADDR 255, 0, 0, 0
#define ETH01_DNS4_ADDR 192, 168, 5, 1
