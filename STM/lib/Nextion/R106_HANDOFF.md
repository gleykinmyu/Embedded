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
| `core/nexStatusMask.hpp` | `kAwaitingPageCommand`, `bkcmdAllowedStatus()` |
| `core/nexSession.hpp` | `Transaction::Kind`, поле `awaiting_status`, `correlatesWith` / `sessionWaitMask` |
| `core/nexSession.cpp` | Реализация `Transaction::*`; correlate в dispatch path |
| `comp/nexAttributes.hpp`, `waveform.hpp`, `nexApplicationAddons.cpp` | маски в enqueue: assign/`add` → `kAwaitingNone`, Page → `kAwaitingPageCommand` |
| `app/nexApplication.cpp` | `correlatesWith`: uncorrelated → `dispatchError(0,0)`, session не `end` |
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
enqueue(Transaction{ cmd, route, kind, mask })  // mask явно или дефолт kAwaitingAllPanel
  → push → emplace(cmd → Slot::storage)
  → _command указывает на storage

begin → pushCommand(queued->command())
  → _status = Active   // active() = *_queue.peek()
```

- `isIdle()` = `_status != Active`.
- `active()` = `*_queue.peek()` (precondition: `!isIdle()`).

### Поведение correlate (текущее)

- Default `awaiting_status = kAwaitingAllPanel` → пока почти всё correlated (как до R106).
- Фоновый status при active session: `onError(0,0)`, session ждёт дальше.
- example6: `kAwaitingPageCommand` на `switchPage`; bench ждёт только `Success` (без `isDataRecordFileNoise`).

### Example6 / bench (ранее в сессии)

- `bkcmd=Always`, drain ACKs, matched Success, `tx=` в summary, Serial1 mute — работало 45/45.
- Не трогали в последних коммитах handoff-ветки.

---

## Обсуждено, но НЕ в коде

| Тема | Решение |
|------|---------|
| `_active` копия в Session | ✓ убрано |
| `Command::defaultAwaitingStatus()` | ✓ убрано; маски в `Transaction` / enqueue |

---

## Следующие шаги (порядок)

1. ~~**Session без `_active`**~~ ✓
2. ~~**PR-2 masks**~~ ✓ (assign/page; file/get — базовый `kAwaitingAllPanel`)
3. ~~**example6**~~ ✓ (`kAwaitingPageCommand` + Success-only wait)
4. ~~**R106f**~~ — **отложено** → [REFACTORING_DEFERRED.md](REFACTORING_DEFERRED.md)
5. ~~**R106d**~~ ✓
6. ~~**R105**~~ ✓ — `getResponseTimeoutMs` / ctor
7. **Следующий:** NEX-R101 (`tryEnqueue`) или PR-3 NEX-R210

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
bool isIdle() const;              // _status != Active
const Transaction& active() const;  // *_queue.peek(), только при Active
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
| 2026-06-18 | PR-1b + PR-2: `_active` убран, masks, ex6 |
| 2026-06-01 | Создан handoff после сессии R106: Kind, wire mask, bkcmd split, EmptyCommand, correlate orphan |
