# DMX_LVGL9 — LVGL 9 + ESP-IDF + LVGL Editor

Playground для **Waveshare ESP32-S3-Touch-LCD-7**: только **ESP-IDF**, без Arduino. UI описывается в XML и правится через **LVGL Editor**.

## Стек

- PlatformIO platform **`espressif32 @ 6.13.0`** (официальный, ESP-IDF **5.5.3**)
- `framework = espidf` only — без Arduino
- Не 7.x: там IDF 6.1, другой API I2C/RGB (порт Waveshare пока под 5.5)
- LVGL **9.x** + локальный `components/esp_lvgl_adapter` (обход бага PlatformIO с `-include`)
- Порт RGB / GT911 / CH422G из Waveshare `09_lvgl_v9_demo`
- UI: `components/ui` — проект LVGL Editor (`project.xml`, `globals.xml`, `screens/`)

## UI (LVGL Editor)

Editor **1.0.x** ищет проект только в `./`, `ui/` или `src/ui/` — не в `components/ui`.
Поэтому в корне есть junction **`ui` → `components/ui`** (создаётся локально, в git не лежит).

### Открыть preview

1. **File → Open Workspace from File…** → `dmx_lvgl9.code-workspace`  
   (или откройте папку `DMX_LVGL9`, если junction `ui` уже есть).
2. Откройте XML: `ui/screens/main_screen.xml` (или `components/ui/screens/main_screen.xml`).
3. `Ctrl+Shift+P` → **LVGL: Open Editor**.
4. Preview справа; правки → **Generate code**.

Если пишет *Cannot open LVGL Editor / no project.xml* — проект не найден. В PowerShell из корня репо:

```powershell
cmd /c mklink /J ui components\ui
```

Логи: **Output → LVGL Editor**.

Исходники UI:

| Файл | Назначение |
|------|------------|
| `components/ui/project.xml` | Дисплей 800×480, имя проекта `ui` (без `lvgl_version`/`theme` — Editor 1.0.x их не знает) |
| `components/ui/globals.xml` | Subjects, цвета, стили |
| `components/ui/screens/main_screen.xml` | Экран (счётчик + кнопка) |
| `components/ui/*_gen.c` | Код после Generate — не править вручную |
| `components/ui/ui.c` | Свои колбэки / observers |

На экране: заголовок **DMX LVGL9**, счётчик `tap_count`, кнопка **Tap me**.

## Сборка / прошивка

USB в разъём **UART**, монитор закрыт:

```powershell
cd C:\GitHub\Embedded\DMX\DMX_LVGL9
C:\Users\User\.platformio\penv\Scripts\pio.exe run -t upload --upload-port COMx
C:\Users\User\.platformio\penv\Scripts\pio.exe device monitor --port COMx
```

Если Editor перезапишет `components/ui/CMakeLists.txt`, верните ветку `ESP_PLATFORM` из git — без неё PlatformIO не соберёт component `ui`.
