# stm — интеграционный пример Nextion

Полноценный PlatformIO-проект: плата STM32F407, `server`, FatFs, HMI sync.

Библиотеки подключаются из общей папки `Embedded/libs/` (`lib_extra_dirs`).

## Сборка

Откройте **эту папку** в Cursor/VS Code (как корень workspace):

```powershell
cd libs/Nextion/examples/stm
pio run -e example2
```

| env | main | app.hpp |
|-----|------|---------|
| example1…7 | `src/mainN.cpp` | `../exampleN/app.hpp` |

## HMI sync

```powershell
python ../../syncHMI/hmi2config.py --update
```

Или из корня репозитория Embedded, указав `--src-dir` и `-i` явно.
