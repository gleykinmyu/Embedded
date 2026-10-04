# ccam

Библиотека **Convertible Protocol v3.05** (Panasonic camera + pan/tilt).

Транспорт — `BIF::IByteStream` (UART / RS-422 / RS-485). DE/RE для half-duplex
делайте внутри реализации потока (`write` / `flush`).

## Подключение

Нужен каталог `libs/Interfaces` (`ibyte_stream.hpp`).

```cpp
#include "ccam.hpp"
#include "devices/devices.hpp"
#include "ibyte_stream.hpp"

uint32_t nowMs(); // монотонные ms

class MyUart : public BIF::IByteStream { /* ... */ };

MyUart uart;
uart.open(ccam::kBaudRate);

ccam::Rs485Transport bus(uart, &nowMs);
ccam::devices::He130Camera camera(bus);
ccam::devices::He130Pt pt(bus);
```

PlatformIO: `lib/ccam` + `lib_extra_dirs` на `Interfaces`.

## Структура каталогов

```
lib/ccam/
├── library.json
├── README.md
├── include/
│   ├── ccam.hpp
│   ├── transport/     # Rs485Transport поверх IByteStream
│   ├── protocol/
│   ├── catalog/
│   └── devices/
├── src/
└── examples/
```

## Примеры

[examples/README.md](examples/README.md) — `ptz_panel`.
