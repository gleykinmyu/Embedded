# ccam

Библиотека **Convertible Protocol v3.05** (Panasonic camera + pan/tilt, RS485).

## Подключение

```cpp
#include "ccam.hpp"
#include "devices/devices.hpp"

class MyHal : public ccam::IRs485Hal { /* UART + DE/RE */ };

ccam::Rs485Transport bus(hal);
ccam::devices::He130Camera camera(bus);
ccam::devices::He130Pt pt(bus);
```

PlatformIO подключает `lib/ccam` автоматически (`include/` + `src/`).

## Структура каталогов

```
lib/ccam/
├── library.json
├── README.md
├── include/                # публичные заголовки (#include "...")
│   ├── ccam.hpp            # umbrella
│   ├── transport/          # RS485, кадры, типы Status
│   ├── protocol/           # сборка кадров Camera / PT
│   ├── catalog/            # enum + таблицы команд (PDF)
│   └── devices/            # CameraDeviceBase, модели HE130, E600, …
├── src/                    # реализация (.cpp)
│   ├── camera_protocol.cpp
│   ├── rs485_transport.cpp
│   ├── camera_device.cpp
│   └── pt_device.cpp
└── examples/               # не входят в сборку библиотеки
    └── ptz_panel/
        └── src/
            ├── main.cpp
            └── app/        # код приложения примера
```

| Каталог | Назначение |
|---------|------------|
| `transport/` | `IRs485Hal`, `Rs485Transport`, `Frame`, `Status` |
| `protocol/` | `camBuild*`, кодирование Oxx / #PT |
| `catalog/` | `CameraCmd`, `PtCmd`, `OsdItem`, `k*Menu[]` |
| `devices/` | высокоуровневый API под модели камер и PT |

## HAL

Реализуйте `ccam::IRs485Hal` — см. `include/transport/rs485_hal.hpp`.

## Примеры

[examples/README.md](examples/README.md) — прошивка `ptz_panel` (джойстик + панель → PTZ).

Сборка из корня репозитория: `pio run -e ptz_panel`.
