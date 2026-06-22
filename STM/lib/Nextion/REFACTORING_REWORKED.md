# Nextion — переработанный backlog

Пункты с **уточнённым scope** или **вынесенной спецификацией**. Активный backlog наряду с [IdMap.md](IdMap.md) и [TRANSPARENT_PROTOCOL.md](TRANSPARENT_PROTOCOL.md). **Отложено:** [REFACTORING_DEFERRED.md](REFACTORING_DEFERRED.md).

**Transparent Data Mode (NIS §1.16):** весь backlog и code TODO — [TRANSPARENT_PROTOCOL.md](TRANSPARENT_PROTOCOL.md) (R301, R302, R215).

**Отложено без переработки:** [REFACTORING_DEFERRED.md](REFACTORING_DEFERRED.md).

**Легенда:** `[ ]` — к реализации; в заголовке — кратко «было → стало».

---

## Фаза 2 — Надёжность очереди и UART

### [x] NEX-R101 — `tryEnqueue()` → `bool` (неблокирующая постановка)

**Было:** side-buffer / deferred FIFO при `QueueFull` (авто re-enqueue при освобождении слота).

**Стало:** неблокирующий `tryEnqueue` + сохранить blocking `enqueue`; без второй очереди.

**Проблема.** Единственный API — blocking `enqueue()`: при полной очереди крутит `update()` до `_timeoutMs`, затем теряет `Transaction` и шлёт `QueueFull`. Из ISR или «fire-and-forget» UI нельзя безопасно ждать; при stall-timeout transport уже **не двигается** — второй FIFO команд это не лечит (head session застрял → любой буфер встанет в тупик).

**Как сейчас** (`nexApplication.cpp`):

```cpp
void enqueue(Transaction tx) { /* spin + stall timeout → dispatchError(QueueFull) */ }
bool Session::enqueue(Transaction tx);  // одна попытка, без update()
```

Example4 намеренно провоцирует QueueFull (65× `sendMe` в `Phase::QueueFull`).

**Цель.**
- `[[nodiscard]] bool tryEnqueue(Transaction tx) noexcept` — **одна** попытка `_session.enqueue(tx)` без цикла `update()`; `true` если принято, `false` если `QueueFull` / иная ошибка Session (→ `dispatchError` с route tx, как сейчас при не-QueueFull).
- Blocking `enqueue()` оставить: внутри — spin `update()` + stall timeout (backpressure / «transport fault»).
- Документ в `nexApplication.hpp`: timeout при `enqueue` = «очередь не продвинулась» → авария, не повод для side-buffer; при `tryEnqueue(false)` приложение само решает retry/drop/coalesce.

**Не делаем:** автоматический side-buffer / deferred FIFO при QueueFull.

**Критерий готовности.** Unit или example: `tryEnqueue` при полной очереди возвращает `false` без блокировки; `enqueue` по-прежнему drain'ит; example4 фаза QueueFull без изменения семантики.

| | |
|---|---|
| **Файлы** | `app/nexApplication.hpp/cpp` |
| **Сложность** | S |
| **Зависит от** | NEX-R106 ✓ |
| **Переработано** | 2026-06-18 — отказ от side-buffer после разбора stall-timeout |
| **Выполнено** | 2026-06-18 — `Application::tryEnqueue`, `Session::tryEnqueue`; `enqueue` через try + spin |

---

### [~] NEX-R103 — Политики UART (`ByteStreamPolicy`)

**Проблема.** Ошибки `IByteStream` и `Gateway` сводятся к одному `dispatchError(appErrorFrom(gw, stream), 0, 0)` в конце `update()`. Нет различия: обрыв линии vs битовая ошибка vs переполнение RX.

**Как сейчас.** `nexErrors.hpp`: `appErrorFrom(BIF::IByteStream::Status)` — маппинг в `msg::Status::AppError`. Recovery (flush RX, reopen port, retry head) — на усмотрение приложения.

**Цель.** `ByteStreamPolicy` / hooks в `Application`:
- `Disconnected` → link-down flag, pause enqueue;
- `BitError` / `FrameError` → flush RX framer, retry active tx с лимитом;
- документировать контракт `IByteStream` для приложения (mock — [R402 DEFERRED](REFACTORING_DEFERRED.md)).

| | |
|---|---|
| **Файлы** | `app/nexErrors.hpp`, `app/nexApplication.cpp`, `Interfaces/iserial.hpp` |
| **Сложность** | L |
| **Перенесено** | 2026-06-18 — spec из REFACTORING_REMAINING |

---

### [ ] NEX-R104 — Настраиваемый размер очереди Session

**Проблема.** `TransactionQueue::kCapacity = 64` и `kMaxObjectSize = 128` — compile-time константы с TODO в `nexSession.hpp`. Embedded-проекты с малым RAM не могут уменьшить; проекты с burst assign — увеличить.

**Как сейчас.** Static storage внутри `TransactionQueue`; example4 упирается в 64 для теста QueueFull.

**Цель.** Template-параметр `Session<Capacity, MaxObjectSize>` или ctor-аргумент с placement storage; default 64/128; `library.json` / docs — как задавать в `Application`.

| | |
|---|---|
| **Файлы** | `core/nexSession.hpp/cpp`, `app/nexApplication.hpp` |
| **Сложность** | M |
| **Перенесено** | 2026-06-18 — spec из REFACTORING_REMAINING |

---

## Фаза 3 — Components / attr API

**Transparent (`addt`, facades `*_t`):** → [TRANSPARENT_PROTOCOL.md](TRANSPARENT_PROTOCOL.md) (NEX-R215, R301, R302).

---

### [ ] NEX-R217 — Mirror attr/SysVar после успешного enqueue

**Проблема.** `attr::Num::operator=` и `SysVar::operator=` пишут зеркало `_val` **до** `enqueueTransaction`. Если enqueue fail (QueueFull после spin, Session fault) — MCU и панель расходятся.

**Как сейчас.** Optimistic update на assign; `applyResponse` / `copy_string_mirror` — только с панели на get.

**Цель (варианты — выбрать при реализации).**
1. **Pessimistic:** обновлять `_val` только после Success status (или после `tryEnqueue` → `true`).
2. **Rollback:** сохранять old value, откат в `onError` по route транзакции.
3. Единая политика для `attr::Num`, `attr::String`, `SysVar`.

**Критерий готовности.** Unit/integration: assign при forced QueueFull → зеркало = старое значение (или откат после fail).

| | |
|---|---|
| **Файлы** | `comp/nexAttributes.hpp`, `app/nexSysVars.hpp`, `app/nexApplication.cpp` |
| **Сложность** | M |
| **Зависит от** | NEX-R101 (для pessimistic через `tryEnqueue`) |
| **Перенесено** | 2026-06-18 — spec из REFACTORING_REMAINING |

---

### [ ] NEX-R218 — ExComponents: полный рефакторинг `DataFile` / `DataRecord` / `FileBrowser`

**Было:** точечная проверка `dis`, `bco2`, `pco2` на `DataFile` (TODO в коде).

**Стало:** **полный рефакторинг** ExComponents-ветки type 65/66 — API, зеркала, mcu-методы по NIS Editor, без «унаследованных» заглушек.

**Проблема.** База `DataFile` смешивает общие поля DataRecord и FileBrowser; `setCellSpacing` → NIS `dis`, `setBco2`/`setPco2` — назначение не сверено; часть attr без зеркал / с сомнительными именами. Листья (`DataRecord`, `FileBrowser`) не выровнены с type 65/66 в `nexComponentBase.hpp`.

**Цель.**
- Сверка **всех** attr type 65/66 с NIS / Editor (не только `dis`/`bco2`/`pco2`).
- Разделить или уточнить базу vs листья: только общие поля в `DataFile`, специфика — в `DataRecord` / `FileBrowser`.
- Переименовать mcu-методы по смыслу UI; удалить ошибочные assign; `onResponse` / зеркала по политике R217.
- Документ в заголовке + при необходимости example (ExComponents сейчас вне example5).

**Критерий готовности.** API без TODO по attr; assign/get для публичных полей; compile + ручная проверка на HMI с DataRecord/FileBrowser.

| | |
|---|---|
| **Файлы** | `comp/nexExComponents.hpp`, `comp/nexComponentBase.hpp` |
| **Сложность** | M / L (полный рефакторинг компонента) |
| **Перенесено** | 2026-06-18 — scope расширен: не точечный fix, а полная выравнивание ExComponents |

---

## Фаза 4 — Application architecture

### [ ] NEX-R204 — Свести `friend class` к минимуму

**Проблема.** `friend`: `SmartApp`, `MsgBox`, `Page`, `nexComponentRegisterFailed`; facades — `friend class Application`. Любое изменение private API ломает несколько модулей.

**Как сейчас.** `SmartApp` — dev-only Discover ([R201 DEFERRED](REFACTORING_DEFERRED.md)); в release остаются `Page`, `MsgBox`, facades, регистрация компонентов.

**Цель.** Аудит каждого friend; заменить на protected hooks / `AppContext` / registration callbacks. Исключение: ctor `Page`/`Component` регистрирует до return — может остаться один narrow friend. `SmartApp` — низкий приоритет, пока не в release.

| | |
|---|---|
| **Файлы** | `app/nexApplication.hpp`, `app/nexSmartApp.hpp`, `comp/nexComponentBase.hpp`, facades |
| **Сложность** | L |
| **Перенесено** | 2026-06-18 — spec из REFACTORING_REMAINING |

---

### [ ] NEX-R206 — `MsgApplication`: MsgBox вне базового `Application`

**Было:** документировать public `lastError()` vs `onError` vs NIS/AppError.

**Стало:** вынести MsgBox в **опциональный** слой — наследник `Application` (рабочее имя `MsgApplication` / `MsgApp`), чтобы прошивка без модальных окон не тащила MsgBox, touch-gate и `friend MsgBox`.

**Проблема.** Сейчас `Application` всегда содержит:
- публичное поле `MsgBox msgBox`;
- `onTouch` / `onTouchXY` с проверкой `msgBox.isActive()`;
- `dispatchEvent` → `onMsgBox`;
- `friend class MsgBox` (доступ к `lastError*` для текста на экране).

Для многих embedded-проектов нужен только `onError` + очередь; MsgBox — лишний flash/RAM и лишний public API.

**Цель.**
1. **Базовый `Application`** — UART, session, pages, `onError`, facades; **без** `msgBox`, без MsgBox-маршрутизации в touch/dispatchEvent.
2. **`MsgApplication : Application`** (или mixin) — добавляет `MsgBox`, перехват touch при активном окне, `onMsgBox`, при необходимости показ ошибки через `lastError*` / canvas.
3. Examples, которым нужен MsgBox, наследуют `MsgApplication`; остальные — `Application`.
4. Документ: ошибки — через **`onError`**; `lastError()` либо остаётся на базе для отладки, либо уходит в Msg-слой (решить при реализации).

**Не делаем:** полный split transport/App (см. [R307](REFACTORING_REWORKED.md#nex-r307--split-nextransport--nexapp-отдельная-проработка)); менять семантику `dispatchError` / dedup.

**Критерий готовности.** example без MsgBox компилируется с `Application` only; example с MsgBox — через `MsgApplication`; `nexApplication.hpp` без `#include nexMsgBox.hpp` в базовом классе.

| | |
|---|---|
| **Файлы** | `app/nexApplication.hpp/cpp`, новый `app/nexMsgApplication.hpp` (имя TBD), `comp/nexMsgBox.hpp`, examples |
| **Сложность** | M |
| **Синергия** | NEX-R204 (меньше friend на базе) |
| **Перенесено** | 2026-06-18 — переформулировано: опциональный MsgBox, не docs-only |

---

## Архитектура — отдельная проработка

Пункты **без готового design pass** — черновик scope, имена и границы API могут измениться. Не брать в спринт без отдельного обсуждения.

### [ ] NEX-R307 — Split `NexTransport` / `NexApp` (отдельная проработка)

**Статус.** Перенесено 2026-06-18 (ранее REFACTORING_REMAINING). Требует отдельной проработки: именование (`NexRuntime` vs **`NexTransport`**), композиция vs наследование, контракт `IErrorSink`, граница с `Session` (Session **не** усложнять).

**Проблема.** `Application` совмещает UART pump, registry страниц, enqueue API, error dispatch, virtual UI hooks — сложно переиспользовать transport без GUI.

**Цель (черновик).** Нижний слой **`NexTransport`** (только pump + очередь); **`NexApp`** добавляет страницы/компоненты/facades. Opt-in для прошивки без `Page`/`Component` и без `ObjStorage` heap.

**Не путать с [NEX-R206](#nex-r206--msgapplication-msgbox-вне-базового-application)** — там только MsgBox в наследнике; R307 — полное разделение transport vs GUI.

#### Слой 1 — `NexTransport` (`app/nexTransport.hpp`) — имя TBD

Владеет тем, что сейчас в `Application` private для шины:

| Сейчас (`Application`) | `NexTransport` |
|---|---|
| `_stream`, `_gateway`, `_session` | те же поля |
| `_clockMsFn`, `_timeoutMs`, `nowMs()` | без изменений |
| `enqueue`, `update`, `pumpUntilIdle` | ядро pump (begin/transmit/pollTimeout/RX loop) |
| `setTimeout` / `getTimeout` | да |
| `clearErrors` для stream/gw/session | да (без `_lastError*`) |
| `bkcmd` (`SysVar`, нужен в `pollTimeout`) | да — единственный sysvar на transport |

**Публичный API transport (черновик):**

```cpp
class NexTransport {
public:
    using ClockMsFn = uint32_t (*)() noexcept;

    NexTransport(BIF::IByteStream& stream, ClockMsFn clockMs, uint32_t timeout_ms = kDefaultTimeoutMs);

    void enqueue(Transaction tx) noexcept;
    void update() noexcept;
    bool pumpUntilIdle() noexcept;

    void setTimeout(uint32_t ms) noexcept;
    uint32_t getTimeout() const noexcept;
    uint32_t nowMs() const noexcept;
    void clearErrors() noexcept;

    SysVar<BkCmd> bkcmd;

protected:
    virtual bool onCorrelatedResponse(const Message& m, const Transaction& active, bool txIdle) noexcept;
    virtual void onAsyncMessage(const Message& m) noexcept;
    virtual void onSessionFault(Session::Status st, Gateway::Status gw, const Transaction* active) noexcept;
    virtual void onTimeout(const Transaction& active) noexcept;
};
```

**Что уходит из transport:** `Rect`/`ScreenLayout`, `Page`/`Component`, facades (`ep`…`sleep`), `sys[]`/`pio[]`, `lastError*`, MsgBox, screen helpers.

#### Слой 2 — `NexApp` (`app/nexApp.hpp`) — композиция или наследование TBD

Всё GUI/registry поверх pump:

| Сейчас (`Application`) | `NexApp` |
|---|---|
| `_pageStorage`, `registerPage`, `getPage`, `getComponent`, `pageCount` | да |
| `_currentPageId`, `onPageChange`, `currentPageId()` | да |
| `ep`, `fs`, `cs`, `audio`, `touch`, `sleep` | да (`friend` → `NexApp` или `ICommandSink`) |
| `sys[3]`, `pio[8]` (+ touch `tch` в facade) | да |
| `lastError*`, `clearErrors` (+ сброс кэша), `dispatchError`, `onError` | да |
| `dispatchResponse` / `dispatchEvent` | override correlated/async hooks |
| `onTouch`, `onTouchXY`, … | virtual hooks |
| screen/sys helpers | thin `enqueue` wrappers |
| `screenLayout()` | да |

**Целевая иерархия (черновик):**

```
NexTransport                    // headless: только pump + callbacks
  └── NexApp                    // composition предпочтительнее наследования — TBD
        └── Application         // alias на переход
              └── SmartApp
              └── MsgApplication   // R206
```

#### Контракт разделения `update()` (черновик)

1. **Transport:** `begin` → `transmit` → `pollTimeout(bkcmd)` → `while (receive)` → correlated vs async; gw/stream fault.
2. **NexApp::onCorrelatedResponse:** текущий `dispatchResponse`.
3. **NexApp::onAsyncMessage:** текущий `dispatchEvent`.
4. **Ошибки:** transport сообщает факт + route tx → **`IErrorSink::onError`**; dedup, `lastError*`, `Page::onError` — в **NexApp** (бывший `dispatchError`). `Session` не трогать.

#### Вопросы для отдельной проработки

- `NexTransport` vs `NexRuntime` — финальное имя.
- Композиция (`NexApp` содержит `NexTransport`) vs `NexApp : NexTransport`.
- `IErrorSink` / `ICommandSink` — один интерфейс или два.
- Нужен ли headless-сценарий в продукте (flash/RAM выигрыш vs стоимость XL-рефактора).
- Связь с R101 (`tryEnqueue` на transport), R204, R206.

#### Критерий готовности (после design pass)

- Headless-прошивка на `NexTransport` без `nexComponentBase` / `ObjStorage`.
- Examples на `NexApp`/`Application` — без изменения поведения.
- Facades на узком sink (`enqueue(Transaction)`).

| | |
|---|---|
| **Файлы** | `app/nexTransport.hpp/cpp`, `app/nexApp.hpp/cpp`; рефактор `nexApplication.*`, facades |
| **Сложность** | XL |
| **Перенесено** | 2026-06-18 — из REMAINING; отложено до отдельной проработки |

---

## Фаза 6 — Инфраструктура / docs

### [ ] NEX-R405 — Стиль комментариев RU/EN в заголовках

**Проблема.** Смесь русских `@file`/`//` и английских идентификаторов без единого правила.

**Цель.** RULE или верхний блок REFACTORING: API docs EN, пояснения протокола RU (или наоборот — один стиль).

| | |
|---|---|
| **Файлы** | `lib/Nextion/**/*.hpp` |
| **Сложность** | S (косметика) |
| **Перенесено** | 2026-06-20 — из REFACTORING_REMAINING |
