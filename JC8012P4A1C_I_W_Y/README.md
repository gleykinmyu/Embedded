# JC8012P4A1C_I_W_Y (Guition)

10.1″ IPS touch panel: ESP32-P4 host + ESP32-C6 Wi‑Fi/BT co-processor (SDIO), JD9365 MIPI-DSI 800×1280, GSL3680 touch.

SKU variants: `JC8012P4A1C_I_W_Y` (touch), `JC8012P4A1C_I_W_Y1` (with battery).

## Official / community links

- Guition product page: https://www.guition.com/esp32p4-display-module/esp32-c6-display
- Schematics & specs (unofficial mirror): https://github.com/p1ngb4ck/unofficial_guition_esp32p4_repo/tree/main/JC8012P4A1C_I_W_Y
- BSP / Getting Started: https://github.com/profi-max/JC8012P4A1_BSP_ESP32P4
- ESP-IDF demos: https://github.com/sukesh-ak/JC8012P4A1-GUITION-ESP32-P4_ESP32-C6

## Local files

| Path | Contents |
|------|----------|
| `specification/` | Spec PDF, Getting Started PDF, product page HTML |
| `schematics/` | PWR / LCD / P4 / CONN / C6 / USB / CODEC sheets (PNG) |
| `structure_diagram/` | Board outline / connector photos & PDF |
| `datasheets/` | ESP32-P4 datasheet, JD9365 init sequence |
| `esphome/` | Example ESPHome YAML |
| `demos/` | Official Guition LVGL Arduino/ESP-IDF demos + community BSP — see `demos/README.md` |

## Pinout summary (ESP32-P4)

### Display / touch

| Function | GPIO |
|----------|------|
| LCD Reset | 27 |
| Backlight PWM | 23 |
| Touch SDA | 7 |
| Touch SCL | 8 |
| Touch RST | 22 |
| Touch INT | 21 |

Display data path: **MIPI-DSI** (JD9365), not parallel RGB.

### TF card (SDMMC)

| GPIO | SD |
|------|-----|
| 43 | CLK |
| 44 | CMD |
| 39–42 | D0–D3 |

### I²C bus (GPIO7/8)

Shared by touch, RTC (RX8025T), ES8311, external CN4 header.

**Warning:** CN4 silkscreen may be mirrored — actual order is often `GND, 3V3, SCL, SDA`.

### Audio I2S (ES8311)

| GPIO | Signal |
|------|--------|
| 13 | MCLK |
| 12 | BCLK |
| 11 | SDOUT |
| 10 | LRCK |
| 9 | DIN |
| 20 | PA enable |

### ESP32-C6 SDIO link

| SDIO | C6 | P4 |
|------|----|----|
| CLK / CMD | IO18 / 19 | GPIO18 / 19 |
| D0–D3 | IO20–23 | GPIO14–17 |
| EN | EN | GPIO54 |
| Wake | IO2 | GPIO6 |

### Other

| Function | GPIO |
|----------|------|
| Battery ADC | 52 |
