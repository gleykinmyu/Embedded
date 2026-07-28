# PUMS Console

PlatformIO-проект: плата STM32F407, Nextion, `server`, FatFs, HMI sync.

Прошивка: McUI overlay (example7) — `src/main.cpp`, приложение в `libs/Nextion/examples/example7/app.hpp`.

Библиотеки подключаются из `Embedded/libs/` (`lib_extra_dirs`).

## Сборка

Откройте **эту папку** в Cursor/VS Code (как корень workspace):

```powershell
cd PUMS/Console
pio run
```

## HMI sync

Перед сборкой PlatformIO может обновить `src/UI/nexHmiConfig.hpp`
(скрипт `syncHMI/pio_hmi_gen.py` из библиотеки Nextion, опции в `platformio.ini`):

| `custom_hmi_gen` | Поведение |
|------------------|-----------|
| `off` / `0` | не генерировать |
| `onchange` / `1` | только если `.HMI` новее `.hpp` (или `.hpp` нет) |
| `always` / `2` | генерировать при каждой сборке |

Имя входа: `custom_hmi_file` (например `src/PUMS 2.5.HMI`).

Вручную:

```powershell
python ../../libs/Nextion/syncHMI/hmi2config.py --update -i "src/PUMS 2.5.HMI" -o "src/UI/nexHmiConfig.hpp"
```

## clangd

```powershell
pio run -t compiledb
copy .vscode\settings.json.example .vscode\settings.json
```
