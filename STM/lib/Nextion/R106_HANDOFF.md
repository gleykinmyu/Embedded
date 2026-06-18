# NEX-R106 — handoff (сессия 2026-06)

Документ для продолжения работы на другом ПК. Полный backlog: [REFACTORING.md](REFACTORING.md).

---

## Перенос диалога Cursor

| Способ | Как |
|--------|-----|
| **Git (рекомендуется)** | Закоммить/запушить ветку с кодом + этот файл. На другом ПК: `git pull`, открыть репо, новый чат, приложить `@lib/Nextion/R106_HANDOFF.md` и нужные файлы. |
| **История чата** | В Cursor: экспорт/копия переписки вручную, если доступно в вашей версии. Полный JSONL транскрипт лежит локально в `.cursor/projects/.../agent-transcripts/` — **не синхронизируется** между машинами автоматически. |
| **Новый чат** | Достаточно этого файла + `REFACTORING.md` + открытые `nexSession.*`, `nexStatusMask.hpp`, `nexApplication.cpp` — контекст восстанавливается за один промпт. |

**Итого:** диалог целиком в другой клиент **не переносится** как живой thread; переносится **код + этот handoff**.

---

## Архитектура (согласованная модель)

Три ортогональных слоя:

| Слой | Где | Смысл |
|------|-----|--------|
| **Тип TX** | `Transaction::Kind` | `Command`, `GetNumeric`, `GetString`, `TransparentTx`, `RawDataRx` |
| **Ждём ли status для session** | `sessionWaitMask(bkcmd)` + `pollTimeout` | Off/OnFailure → `kAwaitingNone` → Complete на `txIdle` |
| **Чей status на проводе** | `awaiting_status` (wire mask) + `statusCorrelatesWithTransaction` | correlated → route tx; иначе `onError(0,0)`, session не `end` |

**NoAwaiting** — не отдельный `Kind`: `awaiting_status = kAwaitingNone` (и/или `bkcmd` Off/OnFailure обнуляет wait-mask).

**Status на UART** может прийти всегда; маска решает «наш / чужой», `Kind` — чем закрыть session (`0x71` vs status vs txIdle).

### NIS `bkcmd` (§6.13 + §7)

- Table 1 (0x00…0x23): Success при 1|3, fail при 2|3, Off — ничего.
- **Не зависит от bkcmd:** `0x24` Serial Overflow → `isBkcmdIndependentStatus()` → never correlate к tx → orphan `(0,0)`.
- Table 2 (`0x70`/`0x71`, touch, …) — не через `awaiting_status`.

### `sessionWaitMask` vs `statusCorrelateMask`

Не эквивалентны: первая — блокировка очереди (pollTimeout), вторая — plausibility для route (в т.ч. fail после txIdle при OnFailure).

---

## Что сделано (PR-1 skeleton, не закоммичено явно в этом файле)

### Новые / изменённые файлы

| Файл | Изменения |
|------|-----------|
| `core/nexStatusMask.hpp` | `AwaitingStatus` (`uint64_t`, bit = wire code), `kAwaitingAllPanel`, `kAwaitingNone`, `bkcmdAllowedStatus()` |
| `core/nexSession.hpp` | `Transaction::Kind`, поле `awaiting_status`, методы mask/correlate в `.cpp` |
| `core/nexSession.cpp` | Реализация `Transaction::*`; correlate в dispatch path |
| `core/nexCommands.hpp/cpp` | `EmptyCommand`, `kEmptyCommand()` — serialize → 0 bytes → Gateway `EmptyPayload` |
| `app/nexApplication.cpp` | `statusCorrelates…`: uncorrelated → `dispatchError(0,0)`, `return true` (session не трогаем) |
| `comp/nexAttributes.hpp`, `app/nexSysVars.*` | `Kind` вместо `State::Awaiting*` |
| `examples/example4/app.hpp` | `Transaction::Kind::Command` / `GetNumeric` |
| `REFACTORING.md` | R106e = mask-based NoAwaiting; R106d частично |

### Переименования

| Было | Стало |
|------|--------|
| `State::AwaitingStatus` | `Kind::Command` |
| `AwaitingNumericGet` / `String` | `GetNumeric` / `GetString` |
| `AwaitingTransparentTx` / `RawDataRx` | `TransparentTx` / `RawDataRx` |
| `State` | `Kind` |
| `nexStatusFilter.hpp` | **удалён** → только `nexStatusMask.hpp` |

### `Transaction` lifecycle

```
enqueue(Transaction{ cmd temp, route, kind, mask })  // by value
  → push → emplace(cmd → Slot::storage)
  → _command указывает на storage

begin → pushCommand(queued->command())
  → _active = *queued   // ⚠ дубль, см. «следующие шаги»
```

- `isEmpty()` = `_command == nullptr` (нет `Kind::Idle`).
- `command()` при null → `kEmptyCommand()`.
- Удалены неиспользуемые `isCommand()` / `expectsReply()`.
- Удалён non-const `Command& command()`.

### Поведение correlate (текущее)

- Default `awaiting_status = kAwaitingAllPanel` → пока почти всё correlated (как до R106).
- Фоновый status при active session: `onError(0,0)`, session ждёт дальше.
- example6: локальные `isDataRecordFileNoise` / `responseMatchesExpected` **ещё не** заменены библиотечной маской.

### Example6 / bench (ранее в сессии)

- `bkcmd=Always`, drain ACKs, matched Success, `tx=` в summary, Serial1 mute — работало 45/45.
- Не трогали в последних коммитах handoff-ветки.

---

## Обсуждено, но НЕ в коде

| Тема | Решение |
|------|---------|
| `_active` копия в Session | Убрать: `active()` → `*_queue.peek()` при `Status::Active`; `isIdle()` → `_status != Active` |
| `active()` при idle | **Precondition:** вызывать только при `!isIdle()`; альтернатива — `tryActive()` → `const Transaction*` |
| `Command::defaultAwaitingStatus()` | Per-class mask при enqueue (PR-2) |
| `switchPage` mask | `awaiting_status` без FileIo (`0x06`) — убрать `isDataRecordFileNoise` в ex6 |
| `last tx` для orphan OnFailure | R106f, example4 |
| `kAwaitingDefault` sentinel | «взять из Command» — не реализовано |

---

## Следующие шаги (порядок)

1. **Session без `_active`** — `active()` = ref на `slots_[head].data`; `end()` только `pop()`.
2. **PR-2 masks** — `Command::defaultAwaitingStatus()`, пресеты (assign/page/file/get); default assign → `kAwaitingNone` (R106e).
3. **example6** — маска на `switchPage` вместо `isDataRecordFileNoise`.
4. **R106f** — `LastCompleted` + orphan fail → `(page, comp)` при OnFailure.
5. **Сборка** — `pio run -e example6` / `example4` на целевой машине (в sandbox CI не гоняли).

---

## Ключевые API (текущие)

```cpp
// Transaction
enum class Kind { Command, GetNumeric, GetString, TransparentTx, RawDataRx };
AwaitingStatus awaiting_status;  // wire mask, kAwaitingNone = NoAwaiting

bool isEmpty() const;
const Command& command() const;

AwaitingStatus sessionWaitMask(BkCmd) const;
AwaitingStatus statusCorrelateMask(BkCmd) const;
bool statusCorrelatesWithTransaction(const msg::Status&, BkCmd) const;

// Session
bool isIdle() const;              // _active.isEmpty() сейчас
const Transaction& active() const;  // _active сейчас
const Transaction* faultingTransaction() const;

// Masks (nexStatusMask.hpp)
constexpr AwaitingStatus kAwaitingNone;
constexpr AwaitingStatus kAwaitingAllPanel;
AwaitingStatus bkcmdAllowedStatus(BkCmd);
bool isBkcmdIndependentStatus(msg::Status::Code);
```

---

## Файлы для @ в новом чате

```
lib/Nextion/R106_HANDOFF.md
lib/Nextion/REFACTORING.md
lib/Nextion/core/nexSession.hpp
lib/Nextion/core/nexSession.cpp
lib/Nextion/core/nexStatusMask.hpp
lib/Nextion/app/nexApplication.cpp
lib/Nextion/examples/example6/app.hpp
```

---

## Журнал handoff

| Дата | Событие |
|------|---------|
| 2026-06-01 | Создан handoff после сессии R106: Kind, wire mask, bkcmd split, EmptyCommand, correlate orphan |
