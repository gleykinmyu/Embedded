#pragma once

#include <stdint.h>

// ESP32-ETH01: LAN8720, тактовая RMII 50 МГц на GPIO0.
// Питание осциллятора PHY: GPIO16 (на части ревизий GPIO17).
static constexpr int PIN_ETH_PHY_POWER = 16;
static constexpr int PIN_ETH_MDC = 23;
static constexpr int PIN_ETH_MDIO = 18;
static constexpr int ETH_PHY_ADDR = 1;

// UART0 (RX0/TX0) — отладка / прошивка / REPL, 115200.
// На разъёме программирования ETH01: TXD0=GPIO1, RXD0=GPIO3.
static constexpr int PIN_DEBUG_TX = 1; // TX0
static constexpr int PIN_DEBUG_RX = 3; // RX0

// UART2 на пинах TXD/RXD модуля (не путать с TXD0/RXD0!).
// WT32-ETH01 / ESP32-ETH01: TXD=GPIO17, RXD=GPIO5. Опционально 485_EN=GPIO33.
static constexpr int PIN_RS422_TX = 17; // TXD  → драйвер RS-422 TX
static constexpr int PIN_RS422_RX = 5;  // RXD  ← приёмник RS-422 RX
static constexpr int PIN_RS422_DE = 33; // 485_EN (если DE/RE; для full-duplex RS-422 не обязателен)
static constexpr int RS422_UART_NUM = 2;
static constexpr int RS422_BAUD = 9600;

/*
 * AW-PH350E / Panasonic P/T CONTROL — RJ45, RS-422A, прямой CAT-5.
 *
 * Источник: «Panasonic cables.pdf» p.36 «Pan/Tilt & Controller Interface»
 * (+ схема CN2 awph350p: HOT/COLD на тех же пинах).
 *
 *  Pin | Pan/Tilt (pdf) | Controller (HOT/COLD) | T-568B
 *  ----+----------------+-----------------------+----------------
 *   1  | GND            | GND                   | бело-оранжевый
 *   2  | —              | —                     | оранжевый
 *   3  | RX-            | COLD2                 | бело-зелёный
 *   4  | TX-            | COLD1                 | синий
 *   5  | TX+            | HOT1                  | бело-синий
 *   6  | RX+            | HOT2                  | зелёный
 *  7–8 | —              | —                     | кор. пара
 *
 * TX*/RX* — со стороны головки (Pan/Tilt).
 * HOT1/COLD1 = пара 1 = TX головки; HOT2/COLD2 = пара 2 = RX головки.
 *
 * Waveshare Port C (как контроллер) → RJ45 головки:
 *   TA → 6 (RX+)   TB → 3 (RX-)
 *   RA ← 5 (TX+)   RB ← 4 (TX-)
 *   GND — 1
 */

#define ETH01_IP4_ADDR  2, 0, 0, 12
#define ETH01_GW4_ADDR  192, 168, 5, 1
#define ETH01_MASK_ADDR 255, 0, 0, 0
#define ETH01_DNS4_ADDR 192, 168, 5, 1
