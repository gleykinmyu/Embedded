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

## Session / `get` (важно)

Ответы панели на `get` (`0x70` / `0x71`) **без ID запроса**. Session считает ответ относящимся к **текущей** активной tx.

Поэтому:

- **Не ставьте в очередь два `get` подряд** (два numeric / string / смесь), если второй может уйти до прихода ответа на первый, либо если после **timeout** первого сразу стартует второй того же вида — запоздалый ответ может закрыть уже новый запрос и подставить чужие данные в UI.
- Практичные варианты: один `get` → дождаться `onResponse` / `onTimeout` → только потом следующий; либо после timeout не полагаться на следующий однотипный `get` без явной паузы / повторного чтения «на всякий случай» перед изменением состояния.
- События touch / page (`0x65` / `0x66`) к этой гонке не относятся — у них другая семантика.

## Include

| Заголовок | Назначение |
|-----------|------------|
| `nex.hpp` | Application, SmartApp, Session, `nexComponents` |
| `comp/nexExComponents.hpp` | DataRecord, FileBrowser, Media… (отдельно) |

Examples: `examples/exampleN/` (заголовки), полный проект — `examples/stm/` (`pio run -e exampleN`).

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
"srcFilter": "+<app/*.cpp> +<comp/*.cpp> +<core/*.cpp> +<smartApp/*.cpp>"
```

**При добавлении нового `.cpp`:** положите файл в одну из этих пап верхнего уровня (`app/`, `comp/`, `core/`, `smartApp/`, `overlay/`) — маска `*/*.cpp` его подхватит. Если нужна **новая папка** или вложенный путь (`app/foo/bar.cpp`) — обновите `library.json`, иначе линковка даст undefined symbol.

## Конфигурация

Размер очереди Session задаётся глобальным compile-time define в `nexConfig.hpp`:

```cpp
#ifndef NEX_SESSION_QUEUE_CAPACITY
#define NEX_SESSION_QUEUE_CAPACITY 64u
#endif
```

Для проекта на PlatformIO можно переопределить его через `build_flags`:

```ini
build_flags =
    -DNEX_SESSION_QUEUE_CAPACITY=16
```

`NEX_SESSION_QUEUE_CAPACITY` должен быть больше `0`. Максимальный размер элемента очереди (`128` байт) пока internal и отдельно не настраивается.

## Комментарии в коде (NEX-R405)

Публичный API в `app/`, `core/`, база `comp/`: **RU** для смысла и протокола; идентификаторы — как в коде в `` `бэктиках` `` (`Application::onStatus`, `0x65`, NIS §…). `.cpp` — только неочевидная логика; массовые листья `nexComponents.hpp` — без per-attr правок.

## Backlog / docs

| Файл | Содержание |
|------|------------|
| [README.md](README.md) | Этот файл — quick start, examples |
| [REFACTORING.md](REFACTORING.md) | Полная история рефакторинга |
| [REFACTORING_REWORKED.md](REFACTORING_REWORKED.md) | Активный backlog (уточнённый scope) |
| [REFACTORING_DEFERRED.md](REFACTORING_DEFERRED.md) | Отложено |
| [TRANSPARENT_PROTOCOL.md](TRANSPARENT_PROTOCOL.md) | `*_t` / `addt` — не для prod до R301 |
| [smartApp/IdMap.md](smartApp/IdMap.md) | Discover / Flash |

**Не использовать в prod:** `ep.write_t`, `fs.file_*_t`, `waveform.addt` — см. [TRANSPARENT_PROTOCOL.md](TRANSPARENT_PROTOCOL.md).
