# DMX_LVGL — устаревший Arduino / LVGL 8 playground

> Актуальный проект: **[`../DMX_LVGL9`](../DMX_LVGL9)** — официальный PlatformIO `espressif32`, чистый ESP-IDF, LVGL 9 (без Arduino).

Ниже — старый вариант на Arduino + LVGL 8 (ESP32_Display_Panel / `lv_demo_widgets()`).

## Стек

- PlatformIO + Arduino (ESP32 3.1.x / pioarduino)
- LVGL **v8.4.0**
- `ESP32_Display_Panel` + `ESP32_IO_Expander`
- Плата: **Waveshare ESP32-S3-Touch-LCD-7** (RGB 800×480, GT911, CH422G)

## Сборка / прошивка

Подключи USB-UART (порт с надписью **UART**), затем:

```bash
cd C:\GitHub\Embedded\DMX\DMX_LVGL
C:\Users\User\.platformio\penv\Scripts\pio.exe run -t upload
C:\Users\User\.platformio\penv\Scripts\pio.exe device monitor
```

Или открой папку как PlatformIO-проект в VS Code / Cursor (`pio run` уже успешно собирается).

> Примечание: `core_dir = C:/pio` в `platformio.ini` — короткий путь, чтобы обойти лимит длины путей Windows. High-perf `esp32-*-h.zip` libs отключены по той же причине; при «дрейфе» RGB можно включить Long Paths в Windows и раскомментировать пакет в ini.

Ожидаемый лог: `Initializing board` → `Initializing LVGL` → `Creating UI: lv_demo_widgets()` → на экране демо с вкладками.

## Что смотреть в коде

| Файл | Зачем |
|------|--------|
| `src/main.cpp` | старт платы + вызов `lv_demo_widgets()` |
| `src/lvgl_v8_port.*` | порт LVGL (буферы, touch, anti-tear) |
| `src/esp_panel_board_custom_conf.h` | пины / RGB / GT911 Waveshare |
| `src/lv_conf.h` | конфиг LVGL (`LV_USE_DEMO_WIDGETS=1`) |
| `platformio.ini` | единственный env `BOARD_CUSTOM` (Waveshare 7″) |

Позже сюда же можно переносить UI DMX-тестера — текущий DMX-проект не трогаем.
