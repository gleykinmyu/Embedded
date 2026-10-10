# DMX_LVGL9 — LVGL 9 + ESP-IDF + lv:: (C++)

Playground для **Waveshare ESP32-S3-Touch-LCD-7**: только **ESP-IDF**, без Arduino.
UI пишется на **C++20** через обёртку [aptumfr/lv](https://github.com/aptumfr/lv) (`Embedded/libs/lv`, AGPL-3.0).

## Стек

- PlatformIO **`espressif32 @ 6.13.0`** (ESP-IDF **5.5.3**)
- LVGL **9.x** + `espressif/esp_lvgl_adapter` из **Component Registry**
- Порт RGB / GT911 / CH422G из Waveshare `09_lvgl_v9_demo`
- UI: папка **`ui/`**

### PlatformIO и `-include`

У `esp_lvgl_adapter` в CMake есть `PUBLIC -include lvgl_port_alignment.h`.  
Сборка через **idf.py** ок; через **PlatformIO/SCons** флаг ломается.

Обход: `Embedded/cmake/pio_strip_public_include.cmake` — после `project()`:

```cmake
include("${_EMBEDDED_ROOT}/cmake/pio_strip_public_include.cmake")
embedded_pio_strip_public_include()
```

Сканирует все компоненты; список имён не нужен. Макрос выравнивания остаётся через `-D LVGL_PORT_PPA_ALIGNMENT`.

## Структура

| Путь | Назначение |
|------|------------|
| `ui/ui.hpp` / `ui.cpp` / `ui.h` | `App` фасад + C entry (`ui_init`) |
| `ui/monitor_screen.*` | Сетка каналов (10×8) |
| `ui/grid_row.hpp` | Ряд сетки: подпись + клетки |
| `ui/graph_screen.*` | График выбранных каналов |
| `ui/tester.hpp` | Модель: страницы, select, trace, scale |
| `ui/chrome.hpp` | ChromeBtn / LinkBadge / ArrowNav |
| `ui/demo_feed.hpp` | Синтетический вход (пока без RS485) |
| `ui/themed_alert.hpp` | MsgBox в стиле UI |
| `../../libs/lv/` | Shared OOP-обёртка `lv::` |
| `main/` | Старт панели + адаптера |
| `managed_components/` | LVGL, esp_lvgl_adapter (registry) |

Монитор + Graph + MsgBox — порт логики STM32/Nextion. Вход пока **DemoFeed** (RS485 позже).

## Сборка / прошивка

```powershell
cd C:\GitHub\Embedded\DMX\DMX_LVGL9
C:\Users\User\.platformio\penv\Scripts\pio.exe run
C:\Users\User\.platformio\penv\Scripts\pio.exe run -t upload --upload-port COMx
```
