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

```powershell
python ../../libs/Nextion/syncHMI/hmi2config.py --update
```

## clangd

```powershell
pio run -t compiledb
copy .vscode\settings.json.example .vscode\settings.json
```
