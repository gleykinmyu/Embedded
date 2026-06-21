# Nextion (MCU library)

C++17-библиотека для Nextion по UART: очередь команд, Session, компоненты HMI, опционально IdMap / SmartApp.

## Быстрый старт

```cpp
#include "nex.hpp"

class MyApp : public nex::Application {
public:
    MyApp(BIF::IHardwareSerial& uart, ClockMsFn ms)
        : Application(uart, {800, 480}, ms)
    {}
};

void loop() {
    app.update();  // pump: TX/RX, timeout, dispatch
}
```

Рекомендуемый **`bkcmd`**: `OnFailure` в prod (ошибки attr без шума Success); `Always` — только для отладки (example4/6).

## Include

| Заголовок | Назначение |
|-----------|------------|
| `nex.hpp` | Application, SmartApp, Session, `nexComponents` |
| `comp/nexExComponents.hpp` | DataRecord, FileBrowser, Media… (отдельно) |

Examples: `lib/Nextion/examples/exampleN/`, сборка — `pio run -e exampleN` (см. корневой `platformio.ini`).

| env | Смысл |
|-----|--------|
| example1 | Страницы, MsgBox |
| example2 | IdMap Discover (default) |
| example3 | Compiled id 1…10 |
| example4 | Автотест ошибок |
| example5 | Все базовые виджеты + demo_controls |
| example6 | Latency bench (HMI как example5) |

## Сборка библиотеки (`library.json`)

Исходники подключаются через `srcFilter`:

```json
"srcFilter": "+<app/*.cpp> +<comp/*.cpp> +<core/*.cpp> +<idmap/*.cpp>"
```

**При добавлении нового `.cpp`:** положите файл в одну из этих пап верхнего уровня (`app/`, `comp/`, `core/`, `idmap/`) — маска `*/*.cpp` его подхватит. Если нужна **новая папка** или вложенный путь (`app/foo/bar.cpp`) — обновите `library.json`, иначе линковка даст undefined symbol.

## Backlog / docs

| Файл | Содержание |
|------|------------|
| [README.md](README.md) | Этот файл — quick start, examples |
| [REFACTORING.md](REFACTORING.md) | Полная история рефакторинга |
| [REFACTORING_REWORKED.md](REFACTORING_REWORKED.md) | Активный backlog (уточнённый scope) |
| [REFACTORING_DEFERRED.md](REFACTORING_DEFERRED.md) | Отложено |
| [TRANSPARENT_PROTOCOL.md](TRANSPARENT_PROTOCOL.md) | `*_t` / `addt` — не для prod до R301 |
| [IdMap.md](IdMap.md) | Discover / Flash |

**Не использовать в prod:** `ep.write_t`, `fs.file_*_t`, `waveform.addt` — см. [TRANSPARENT_PROTOCOL.md](TRANSPARENT_PROTOCOL.md).
