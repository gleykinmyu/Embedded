#pragma once

#include <stddef.h>
#include <stdint.h>

// Waveshare ESP32-S3-POE-ETH-8DI-8RO / ESP32-S3-ETH-8DI-8RO
// Реле — EXIO1..EXIO8 на TCA9554PWR (I2C 0x20), не GPIO MCU.

static constexpr int PIN_RGB = 38;
static constexpr int PIN_BUZZER = 46;
static constexpr int PIN_I2C_SDA = 42;
static constexpr int PIN_I2C_SCL = 41;

static constexpr int PIN_ETH_INT = 12;
static constexpr int PIN_ETH_MOSI = 13;
static constexpr int PIN_ETH_MISO = 14;
static constexpr int PIN_ETH_SCLK = 15;
static constexpr int PIN_ETH_CS = 16;

#define ETH_IP4_ADDR   192, 168, 6, 202
#define ETH_GW4_ADDR   192, 168, 5, 1
#define ETH_MASK_ADDR  255, 255, 248, 0
#define ETH_DNS4_ADDR  192, 168, 5, 1

static constexpr int PIN_DI[] = {4, 5, 6, 7, 8, 9, 10, 11};
static constexpr uint8_t PIN_RELAY[] = {0, 1, 2, 3, 4, 5, 6, 7};
static constexpr size_t RELAY_COUNT = sizeof(PIN_RELAY) / sizeof(PIN_RELAY[0]);

static constexpr uint8_t TCA9554_ADDR = 0x20;
static constexpr uint8_t TCA_REG_OUTPUT = 0x01;
static constexpr uint8_t TCA_REG_CONFIG = 0x03;

#ifndef RELAY_STEP_MS
#define RELAY_STEP_MS 500
#endif
