# ESP32-S3-Touch-LCD-7 (Waveshare)

7″ capacitive touch HMI board: ESP32-S3N16R8, RGB LCD 800×480, GT911 touch, CH422G IO expander.

## Official links

- Wiki: https://docs.waveshare.com/ESP32-S3-Touch-LCD-7
- User Guide: https://docs.waveshare.com/ESP32-S3-Touch-LCD-7/Instructions-For-Use
- Resources: https://docs.waveshare.com/ESP32-S3-Touch-LCD-7/Resources-And-Documents
- Product page: https://www.waveshare.com/esp32-s3-touch-lcd-7.htm
- Examples (GitHub): https://github.com/waveshareteam/ESP32-S3-Touch-LCD-7

## Local files

| Path | Contents |
|------|----------|
| `schematics/` | Board schematic PDF |
| `drawings/` | Mechanical drawings (with/without touch) |
| `datasheets/` | ESP32-S3, WROOM-1, GT911, CH422G, ST7262, CH343 |
| `manuals/` | ESP32-S3 TRM + saved Waveshare wiki HTML pages |
| `demos/` | Official Waveshare examples incl. Arduino/ESP-IDF LVGL v8/v9 — see `demos/README.md` |

## Pinout summary

### LCD RGB565

| ESP32-S3 | LCD | ESP32-S3 | LCD |
|----------|-----|----------|-----|
| GPIO0 | G3 | GPIO38 | B4 |
| GPIO1 | R3 | GPIO39 | G2 |
| GPIO2 | R4 | GPIO40 | R7 |
| GPIO3 | VSYNC | GPIO41 | R6 |
| GPIO5 | DE | GPIO42 | R5 |
| GPIO7 | PCLK | GPIO45 | G4 |
| GPIO10 | B7 | GPIO46 | HSYNC |
| GPIO14 | B3 | GPIO47 | G6 |
| GPIO17 | B6 | GPIO48 | G5 |
| GPIO18 | B5 | CH422G EXIO2 | DISP (backlight) |
| GPIO21 | G7 | CH422G EXIO3 | LCD_RST |

### Touch / I²C / SD / buses

| Function | Pin |
|----------|-----|
| TP_IRQ | GPIO4 |
| TP_SDA / I²C SDA | GPIO8 |
| TP_SCL / I²C SCL | GPIO9 |
| TP_RST | CH422G EXIO1 |
| SD MOSI / SCK / MISO | GPIO11 / 12 / 13 |
| SD_CS | CH422G EXIO4 |
| RS485 TX / RX | GPIO15 / 16 |
| USB D−/D+ or CAN RX/TX | GPIO19 / 20 |
| USB/CAN select | CH422G EXIO5 (Low=USB, High=CAN) |
| ADC | GPIO6 |
| UART0 TX / RX | GPIO43 / 44 |

I²C addresses in use: CH422G `0x24`, GT911 `0x5D` (or `0x14`).
