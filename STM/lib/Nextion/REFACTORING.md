# Nextion — план рефакторинга

Живой backlog для поэтапного улучшения библиотеки `lib/Nextion`.
Отмечайте выполненное: `[x]`, в работе: `[~]`, отменено: `[-]`.

**Как идти:** с **глубины кода** (`core/` → `comp/` → `app/`) — см. раздел «Bottom-up порядок» ниже. Фазы 2–4 — после закрытия базового слоя.

---

## Bottom-up порядок (ядро → приложение)

| Слой | Модули | Пункты backlog |
|------|--------|----------------|
| **0. Типы и протокол** | `nexTypes.hpp`, `nexProtocol.hpp` | NEX-R002 ✓ |
| **1. Ошибки и политики** | `app/error/*` | NEX-R102 ✓, NEX-R103 ~ |
| **2. Session / Gateway** | `nexSession.*`, `nexGateway.*` | NEX-R104, NEX-R101 |
| **3. Commands / Messages** | `nexCommands.*`, `nexMessages.hpp` | NEX-R403 |
| **4. SysVar helpers** | `nexSysVars.*` | NEX-R009 ✓ |
| **5. Components** | `comp/*` | NEX-R204 (friend) |
| **6. Application / CompIdMap** | `app/*` | NEX-R001…R010, R201… |
| **7. Examples / tests** | `examples/`, host tests | NEX-R011, NEX-R401 |

**Следующие шаги (слой 2):** NEX-R104 (очередь Session), NEX-R105 (timeout), NEX-R103 (ByteStreamPolicy).

---

## Модель ошибок (`app/error/nexErrors.hpp`)

Три оси + процедуры (не смешивать «слой» и «объект»).

| Роль | Что это | В коде |
|------|---------|--------|
| **Транспортный слой** | ByteStream → Gateway → Session | `ErrorReporter::Stream/Gateway/Session` |
| **Панель** | Nextion, NIS-ответы | `msg::Status`; `dispatchError` без recovery |
| **Домен** | `MISC::ObjRegistry` (Page, Component) | `ErrorReporter::Domain` + `MISC::RegStatus` |
| **Операция** | Сценарий Application | `AppOperation` / `AppStatus` → `tag_1` |
| **Процедура** | CompIdMap Discover и т.п. | `AppProcedure`; не reporter |

**CompIdMap — не слой.** Режим `Discover` = `AppProcedure::CompIdMapDiscover` при старте: ставит транзакции с маршрутом `Route::kCompIdMapPoll*` (0xFE/0xFE), использует Session/Gateway. Сбой → `pollFail` / `onCompIdMapComplete(false)`; таймаут сессии на 0xFE **не** эскалируется в `AppStatus::SessionTimeout` (NEX-R007). Reporter по-прежнему Session/Gateway/Panel, если нужен разбор UART.

### Матрица путей (фактический код)

| Источник сбоя | Reporter / канал | Operation (`AppStatus`) | Subject |
|---------------|-------------------|-------------------------|---------|
| `tryEnqueue` false | Session (+ Command при emplace) | `EnqueueRejected` | `Transaction` / route из `tx` |
| `Session::begin` / activate | Session (`NotIdle`, `QueueEmpty`, …) | `SessionActivateFailed` | — |
| `Session::begin` push fail | Session `PushFailed` + Gateway/Command | `GatewayPushFailed` | active `Transaction` |
| `Session::transmit` fail | Session `TransmitFailed` + Stream/Gateway | `GatewayTransmitFailed` | active `Transaction` |
| RX tail в `update` | Stream или Gateway | `GatewayReceiveFailed` | active `Transaction` |
| `Session::checkResponseTimeout` (не 0xFE) | Session `ResponseTimedOut` | `SessionTimeout` | active `Transaction` |
| timeout (0xFE) | — → CompIdMap | — | `pollFail` |
| `registerPage` / `registerComponent` | Domain | `Page/ComponentRegisterFailed` | Page / Component |
| `dispatchResponse` NIS Status | Panel (`msg::Status`) | — | route транзакции |
| `dispatchEvent` Status | Panel | — | (0,0) глобальный |
| CompIdMap `pollFail` | — (debug reason) | — | `onCompIdMapComplete(false)` |

`Command::Status` в wire: reporter **Command**, но слой-инициатор — Session или Gateway.

---

## Сводка по модулям

| Модуль | Файлы | Состояние |
|--------|-------|-----------|
| **Application** | `app/nexApplication.*`, `app/error/*`, `nexApplicationAddons.cpp` | Рабочий UART-цикл; ошибки в `AppErrorHandler` |
| **CompIdMap** (`Application::idMap`) | `app/nexCompIdMap.*` | Процедура Discover при старте; не transport layer (см. модель ошибок) |
| **Ошибки** | `app/error/nexErrors.hpp`, `app/error/nexAppErrorHandler.*` | Политики очереди; link-down частично; TODO QueueFull, BitError |
| **Session** | `core/nexSession.*` | Очередь 64×128 B; hardcoded capacity |
| **Gateway** | `core/nexGateway.*` | RX/TX framer стабильны |
| **Commands** | `core/nexCommands.*` | Большой NIS-слой; утилита debug TX |
| **Components** | `comp/*` | Page/Component registry; много `friend` |
| **Facades** | `app/nexApplicationFacades.*` | ep/fs/audio/sleep/touch; transparent API — заглушки |
| **Examples** | `examples/example{1,2,3}/` | Нет примера Flash/NXCI; example3 — минимальный |

---

## Фаза 1 — Quick wins (низкий риск, высокая польза)

- [x] **NEX-R001** — Публичный `Application::idMap` (`CompIdMap`)
  - **Что:** `CompIdMap idMap`; `loadFromBuffer`, `applyFromTable`, `table()` на `idMap`.
  - **Файлы:** `nexApplication.hpp`, `nexCompIdMap.*`.
  - **Сложность:** S

### API и CompIdMap

- [ ] **NEX-R001b** — Пример Flash-режима (см. NEX-R011)

- [x] **NEX-R002** — Единый источник констант спец-маршрутов
  - **Что:** `enum class ReservedRoute` или `namespace Route` в `core/nexTypes.hpp`: SysVar `0xFF/0xFF`, Cid `0xFE/0xFE`.
  - **Зачем:** сейчас дубли `kSysVarRoute*`, `kCidRoute*`, `Cid::kRoute*`.
  - **Файлы:** `nexTypes.hpp`, `nexApplication.hpp`, `nexCompId.hpp`, call sites.
  - **Сложность:** S

- [x] **NEX-R003** — Отключить IdMap debug по умолчанию
  - **Что:** `kCompIdMapPollDebug` только при `-DNEX_IDMAP_DEBUG` (см. `NEX_DBG_IDMAP` в `nexDebug.hpp`).
  - **Файлы:** `nexCompIdMap.cpp`.
  - **Сложность:** S

- [x] **NEX-R004** — Убрать `static` из `CompIdMap::pollTick` (SwitchPage)
  - **Что:** `switch_page_last_log_ms` / `switch_page_last_sendme_ms` в `CompIdMapPoll`.
  - **Файлы:** `nexCompIdMap.hpp`, `nexCompIdMap.cpp`.
  - **Сложность:** S

### Ошибки и уведомления

- [x] **NEX-R005** — удалён `showErrorBox` (используйте `dispatchError` + `msgBox.show` / `showError`)
  - **Что:** deprecated-метод не должен дублировать запись `_lastError*` в обход `dispatchError`.
  - **Файлы:** `nexApplication.hpp` (inline → cpp или делегирование).
  - **Сложность:** S

- [x] **NEX-R006** — `clearErrors()` сбрасывает `_lastError*`
  - **Что:** добавить сброс `_lastError`, `_lastErrorPage`, `_lastErrorComp` (сейчас только `_lastStatus` + subsystems).
  - **Файлы:** `nexAppErrorHandler.*`.
  - **Сложность:** S

- [x] **NEX-R007** — Не дублировать `onError` при CompIdMap SessionTimeout
  - **Что:** если маршрут `0xFE/0xFE` и `pollFail` уже вызван — не слать второй `AppError SessionTimeout` (или сделать опциональным).
  - **Файлы:** `nexApplication.cpp` (`checkSessionTimeout`).
  - **Сложность:** S

- [ ] **NEX-R008** — Вынести форматирование ошибок из заголовка
  - **Что:** `formatStatusMessage`, `printStatusError`, `applicationStatusCstr` → `app/nexErrorFormat.cpp`.
  - **Зачем:** уменьшить `nexApplication.hpp`, убрать `<cstdio>` из публичного заголовка.
  - **Файлы:** новый `nexErrorFormat.{hpp,cpp}`, `nexApplication.hpp`, `library.json` srcFilter.
  - **Сложность:** M

### Дублирование кода

- [x] **NEX-R009** — Общий helper `enqueueSysVarNumericAssign`
  - **Что:** одна функция вместо 3 копий в `nexApplicationAddons.cpp`, `nexApplicationFacades.cpp`, `nexCanvas.cpp`.
  - **Куда:** `app/nexSysVars.cpp` или `app/nexEnqueueHelpers.cpp` (internal header).
  - **Сложность:** S

- [x] **NEX-R010** — Убрать debug-логи IdMap из `nexApplication.cpp`
  - **Что:** `std::printf` в `checkSessionTimeout` / `dispatchResponse` за `#ifdef NEX_CID_DEBUG` или удалить (логи уже в `nexCompId.cpp`).
  - **Файлы:** `nexApplication.cpp`.
  - **Сложность:** S

### Примеры и документация

- [ ] **NEX-R011** — Пример Flash-режима (NXCI)
  - **Что:** `example4` или расширение `example2`: Discover → `cidTableEncode` → «EEPROM» буфер → reboot → `loadFromBuffer`.
  - **Файлы:** `examples/`, `platformio.ini`.
  - **Сложность:** M

- [ ] **NEX-R012** — Краткий README библиотеки
  - **Что:** режимы Cid, цикл `update`, обработка ошибок, минимальный sketch.
  - **Файлы:** `lib/Nextion/README.md`.
  - **Сложность:** S

---

## Фаза 2 — Надёжность очереди и UART

### QueueFull и retry (существующие TODO)

- [ ] **NEX-R101** — Буфер отложенных транзакций при `QueueFull`
  - **Что:** не терять `Transaction` при `tryEnqueue` false; повтор после `sessionEnd` / освобождения слота.
  - **TODO:** `nexApplication.hpp:50`, `nexAppErrorHandler.cpp` (`handleEnqueueFailure`).
  - **Файлы:** `nexApplication.cpp`, `nexAppErrorHandler.*`, возможно `Session`.
  - **Сложность:** L
  - **Зависит от:** —

- [x] **NEX-R102** — Лимит повторов `ResetActive` на голову очереди
  - **Что:** счётчик на active tx; после N неудач — `DropHead` или новый `Application::Status`.
  - **TODO:** `nexAppErrorHandler.cpp` (`applyQueueRecovery`), `nexErrors.hpp`.
  - **Сложность:** M
  - **Зависит от:** —

- [~] **NEX-R103** — Политики UART (`ByteStreamPolicy`)
  - **Что:**
    - `Disconnected` → режим link-down (`Application::isLinkDown()`, halt TX/enqueue) — **частично в `AppErrorHandler`**.
    - `BitError` / `FrameError` → flush RX, optional re-init baud, retry с лимитом — **ещё TODO**.
  - **TODO:** `nexErrors.hpp:142–149`, `nexAppErrorHandler.cpp` (`handleGatewayStreamFailure`).
  - **Файлы:** `nexErrors.hpp`, `nexAppErrorHandler.*`, `nexApplication.hpp`.
  - **Сложность:** L

- [ ] **NEX-R104** — Настраиваемый размер очереди Session
  - **Что:** `TransactionQueue::kCapacity` / `kMaxObjectSize` — template param или constexpr в `Application`.
  - **Зачем:** embedded-проекты с разным RAM budget.
  - **Файлы:** `nexSession.hpp`, `nexSession.cpp`.
  - **Сложность:** M

- [ ] **NEX-R105** — Настраиваемый `kDefaultGetResponseTimeoutMs`
  - **Что:** параметр конструктора `Application` или setter.
  - **Файлы:** `nexApplication.hpp`, `nexApplication.cpp`.
  - **Сложность:** S

---

## Фаза 3 — Связность и архитектура Application

### Развязка Cid ↔ Application

- [ ] **NEX-R201** — `CidHost` вместо `friend` + прямого доступа к `_pageCount`, `_session`
  - **Что:** тонкий фасад с методами `pageCount()`, `currentPageId()`, `sessionIdle()`, `hasQueued()`, `enqueue()`, `switchPage()`, `requestCurrentPage()`.
  - **Файлы:** `nexCompId.*`, `nexApplication.hpp`.
  - **Сложность:** M

- [x] **NEX-R202** — Выделить `AppErrorHandler`
  - **Что:** `notifyAppError`, `finalizeAppFailure`, `applyQueueRecovery`, `_lastStatus`, `_lastError*`, `handle*Failure`; виртуальные `*FailurePolicy` остаются на `Application`.
  - **Файлы:** `app/error/*` (заменяет `nexApplicationErrors.cpp`).
  - **Сложность:** M

- [x] **NEX-R203** — Единый `AppFailure` + `AppErrorHandler::handle` + `queuePolicy`
  - **Что:** `AppFailure` + `handle()`, `defaultQueuePolicy(ErrorDetail)`, builders в `app/error/nexAppErrorHandler.cpp`; `queuePolicy(const AppFailure&)`.
  - **Файлы:** `app/error/*`, `nexApplication.*`.

- [x] **NEX-R203a** — `AppFailure`: recovery/clear внутри handler (шаг A)
  - **Что:** из struct убраны `FailureRecovery` и флаги `clear_*`; `recoveryModeFor(operation)` + `clearLayerErrors(operation)` в `nexAppErrorHandler.cpp`.
  - **Далее (шаг B):** `handleTransport` / `notifyApp`, `queuePolicy(ErrorDetail)`.

- [x] **NEX-R203b** — `Session`: `begin` / `transmit` / `end`, fault в `update`
  - **Что:** UART-шаги в `Session` (`Gateway&`); `Session::Status` + `hasFaultStatus()`; `Application::processSessionFaults()` / `finishSession()`; коды `PushFailed`, `TransmitFailed`, `ResponseTimedOut`.
  - **Файлы:** `core/nexSession.*`, `app/nexApplication.*`.

- [ ] **NEX-R204** — Свести `friend class` к минимуму
  - **Аудит:** `Application` ↔ `Cid`, `Page`, `MsgBox`; `Page` ↔ `Application`, `Cid`; facades ↔ `Application`.
  - **Цель:** публичные narrow API вместо friend там, где возможно.
  - **Сложность:** L

### dispatch и маршрутизация

- [ ] **NEX-R205** — Таблица маршрутов вместо if-цепочек в `dispatchResponse`
  - **Что:** helper `isCidRoute(tx)`, `isSysVarRoute(tx)` + единый switch по `Transaction::State`.
  - **Файлы:** `nexApplication.cpp`.
  - **Сложность:** M

- [ ] **NEX-R206** — `lastStatus()` для NIS-ошибок
  - **Что:** при `dispatchError` с панельным `msg::Status` — опционально маппить в `Application::Status` или документировать, что `lastStatus` только для AppError.
  - **Сложность:** S (док) / M (код)

---

## Фаза 4 — Незавершённые фичи и крупные дыры

### Transparent / raw data (критичный пробел)

- [ ] **NEX-R301** — Реализовать `AwaitingTransparentTx` / `AwaitingRawDataRx` в `dispatchResponse`
  - **Что:** сейчас состояния объявлены и используются в `AppEeprom::write_t/read_t`, `AppFileSystem::file_*_t`, но **нет обработки** в `Application::dispatchResponse` — только `AwaitingNumericGet`, `AwaitingStringGet`, `AwaitingStatus`.
  - **Нужно:** интеграция с `Gateway::writeTransparentRaw`, `evTransparent`, приём raw bytes, колбэки в facades.
  - **Файлы:** `nexApplication.cpp`, `nexGateway.*`, `nexApplicationFacades.*`.
  - **Сложность:** XL
  - **Приоритет:** высокий, если используются ep/fs `_t` API

- [ ] **NEX-R302** — Параметры `buffer` в `write_t` / `read_t` не используются
  - **Что:** `(void)buffer` в facades — заглушки; связано с NEX-R301.
  - **Сложность:** XL (вместе с R301)

### Cid — производительность и UX

- [ ] **NEX-R303** — Индекс `(page_id, name)` для `applyFromTable` / `CidTable::find`
  - **Что:** при `count > ~32` линейный поиск по всем страницам дорогой.
  - **Сложность:** M

- [ ] **NEX-R304** — Прогресс Discover для UI
  - **Что:** `Cid::pollProgress()` — `(done, total)` слотов; использовать в `onCidPollComplete` / экране загрузки.
  - **Сложность:** M

- [ ] **NEX-R305** — Валидация `panel_id` после опроса
  - **Что:** отклонять `0xFF`, `kPageCompId`, дубликаты на одной странице с понятным `pollFail`.
  - **Сложность:** S

### Application — размер класса

- [x] **NEX-R306** — Удалить deprecated `showErrorBox`
  - **После:** миграция примеров на `msgBox.showError`.
  - **Сложность:** S

- [ ] **NEX-R307** — Optional: разделить `NexRuntime` / `NexApp`
  - **Что:** UART/session/gateway vs pages/UI/facades — только при существенном росте кодовой базы.
  - **Сложность:** XL

---

## Фаза 5 — Качество, тесты, инфраструктура

- [ ] **NEX-R401** — Host unit-тесты (PlatformIO native / Catch2)
  - **Кандидаты:** `CidTable` encode/decode, `defaultQueuePolicy`, `RxFramer`, `TranslateMessage`, `Session` queue.
  - **Сложность:** L

- [ ] **NEX-R402** — Mock `IByteStream` для тестов Application
  - **Сложность:** M
  - **Зависит от:** NEX-R401

- [x] **NEX-R403** — Единый макрос/флаг debug: `NEX_DEBUG`, `NEX_CID_DEBUG`, `NEX_TRACE_TX`
  - **Что:** `core/nexDebug.hpp` — `NEX_DBG`, `NEX_DBG_IDMAP`, `NEX_DBG_TRACE_TX`; `-DNEX_DEBUG` включает IdMap + TX trace.
  - **Файлы:** `nexCommands.cpp`, `nexCompIdMap.cpp`, `nexApplication.hpp`, примеры, `main*.cpp`.
  - **Сложность:** S

- [ ] **NEX-R404** — Проверка `library.json` srcFilter после новых `.cpp`
  - **Сложность:** S

- [ ] **NEX-R405** — Согласовать стиль комментариев (RU/EN) в заголовках модулей
  - **Сложность:** S (косметика)

---

## Известные архитектурные решения (не трогать без причины)

| Решение | Почему оставить |
|---------|-----------------|
| Не вызывать `setId()` в `onPollResponse` | swap в `_registry` ломает tag=slot |
| Страницы `0..N-1` подряд для Discover | упрощает машину опроса |
| `QueuePolicy::NotifyOptional` + `notifyOptional()` | шум NotIdle/QueueEmpty в debug |
| `msg::Status` + `AppError` в одном `onError` | единая точка UI |
| `friend` для регистрации Page/Component | ctor регистрирует страницу до return |

---

## Рекомендуемый порядок первых 5 PR (bottom-up)

1. ~~**NEX-R002** + **NEX-R009**~~ — `Route` в `nexTypes`, helper в `nexSysVars` ✓  
2. ~~**NEX-R006** + **NEX-R102**~~ — `clearErrors`, retry-лимит ✓  
3. **NEX-R104** + **NEX-R105** — настраиваемая очередь и timeout Session  
4. **NEX-R103** — ByteStreamPolicy / link-down  
5. ~~**NEX-R003** + **NEX-R004** + **NEX-R005** + **NEX-R007** + **NEX-R010**~~ — CompIdMap / ошибки quick wins ✓

---

## Журнал изменений

| Дата | ID | Статус | Комментарий |
|------|-----|--------|-------------|
| 2026-05-27 | NEX-R403 | done | nexDebug.hpp, NEX_DBG / NEX_DBG_IDMAP / NEX_DBG_TRACE_TX |
| 2026-05-27 | NEX-R003…R010, R102 | done | clearErrors, retry limit, IdMap poll fixes |
| 2026-05-27 | rename | done | `Cid` → `CompIdMap`, `Application::idMap`, `nexCompIdMap.*` |
| 2026-05-27 | — | создан | Первичный аудит `lib/Nextion` (34 файла) |
