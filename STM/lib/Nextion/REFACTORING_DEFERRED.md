# Nextion — отложенный backlog

Пункты, сознательно **не в активном** backlog ([REFACTORING_REWORKED.md](REFACTORING_REWORKED.md), [IdMap.md](IdMap.md)): не блокируют core, низкий приоритет или ждут продуктового решения.

История и контекст R106: [R106_HANDOFF.md](R106_HANDOFF.md), полный план: [REFACTORING.md](REFACTORING.md).

**Легенда:** `[-]` — отложено.

---

## Фаза 1 — Session / UART

### [-] NEX-R106f — example4: маршрут OnFailure к `(page, comp)`, не orphan `(0,0)`

**Проблема.** После R106 uncorrelated panel-status уходит в `dispatchError(…, 0, 0)` — это правильно для фонового шума. Но при `bkcmd=OnFailure` и ошибке **по текущей** serial-команде (invalid attr на известном компоненте) UI и example4 ожидают `(page_id, comp_id)` активной транзакции, а не orphan.

**Как сейчас.** `dispatchResponse` для `Kind::Command`: correlated fail → `dispatchError(*st, active->page_id, active->comp_id)`. Если status пришёл **после** txIdle при OnFailure (fail-bit в correlate-маске, но session уже Complete) — маршрут теряется. Example4 в фазах NIS-тестов считает `stats.nis_panel` по любому `(page, comp)`, но сценарий «invalid attr assign + OnFailure» может дать `(0,0)`. То же для waveform `add` с `kAwaitingNone` (R214): fail 0x12 → orphan до реализации last-tx.

**Цель.** Хранить «последнюю завершённую tx» (`LastCompleted` / `lastTx`) с `(page, comp, route, mask)` и при orphan fail-status, plausibly matching last command, маршрутизировать к last route вместо `(0,0)`.

**Критерий готовности.** Example4: тест «invalid attr на b0 при OnFailure» → `onError` с `(kPageId, 1)`, не `(0,0)`; регрессия example6 bench не ломается.

| | |
|---|---|
| **Файлы** | `core/nexSession.hpp/cpp`, `app/nexApplication.cpp`, `examples/example4/app.hpp` |
| **Сложность** | S |
| **Зависит от** | NEX-R106 ✓ |
| **Отложено** | 2026-06-18 — UX example4 / orphan fail-route; не блокирует очередь и masks |

---

## Фаза 3 — Components / attr API

### [-] NEX-R212 — `TextSelect`: без зеркала `txt`, достаточно `val`

**Проблема.** В NIS у TextSelect есть RO-атрибут `txt` (текст выбранной строки). В коде остались закомментированные `attr::StringRO txt` и `onResponse(Txt)` — выглядит как незавершённая фича.

**Решение.** Зеркалить `txt` на MCU **не нужно**: список задаётся через `path` (файл на SD), строки знает приложение; выбранная позиция — `val` (индекс), его зеркало уже в `ListSelect`. Get `txt` с панели дублирует то, что MCU и так контролирует.

**Цель (когда дойдём).** Убрать мёртвый/заглушечный код; задокументировать в `TextSelect` и README: API — `path`, `val`, оформление (`setSelColor`, …); без `txt` в MCU.

| | |
|---|---|
| **Файлы** | `comp/nexComponents.hpp`, `examples/example5/README.md` |
| **Сложность** | S |
| **Отложено** | 2026-06-18 — продуктовое решение принято; cleanup не блокирует demo |

---

## Фаза 4 — Application architecture

### [-] NEX-R201 — SmartApp: dev-only Discover, не рефакторить под prod

**Контекст.** `SmartApp` нужен на этапе разработки HMI: опрос panel id (Discover) и выдача корректного массива `compiled_id` ↔ `panel_id` для IdMap. **В выпускаемом ПО SmartApp не используется** — в firmware остаётся только `Application` + заранее прошитая/загруженная таблица id.

**Было в backlog.** `ISmartAppHost` / narrow API вместо `friend` + доступа к `_session` — рефакторинг под тестируемость prod-стека.

**Отложено вместо этого.** Возможно, стоит **пересмотреть форму взаимодействия** с SmartApp (отдельный dev-tool / example-only слой, не «второй Application»), а не вычищать `friend` ради prod. Тесная связь `SmartApp` ↔ `Application` для Discover допустима, пока модуль не входит в release.

**Когда вернуться.** Если понадобится Discover в поле, CI без панели, или вынос IdMap в отдельный пакет — тогда выбрать новый контракт (не обязательно `ISmartAppHost` из старого R201).

| | |
|---|---|
| **Файлы** | `app/nexSmartApp.*`, `app/nexApplication.hpp`, `examples/example4` |
| **Сложность** | M (старый scope) / ? (новая форма — TBD) |
| **Отложено** | 2026-06-18 — SmartApp не в release; refactor friend не в приоритете |

---

## Фаза 6 — Тесты и инфраструктура

### [-] NEX-R401 — Host unit-тесты (PlatformIO native)

**Что это было в backlog.** Отдельная сборка `[env:native]` на **ПК** (не STM32): маленькие `main()` + Unity/Doctest, которые **без UART и без панели** прогоняют куски чистой логики — например `idmap::Table::encode`/`decode`, разбор байт в `RxFramer`, таблицу `Transaction::correlatesWith(bkcmd, mask)`.

**Чем это не является.** Это **не** замена example4/example6 на реальной Nextion. Интеграция (UART, тайминги, panel status, очередь под нагрузкой) на ПК **не проверяется** — для этого нужна плата или тяжёлый mock UART (→ R402).

**Почему для этого репо не берём.** Регрессия уже on-target: example4 (ошибки), example6 (latency), логи `NEX_DEBUG`. Поднять native env = второй toolchain, вырезать/заглушить `IByteStream`, HAL, часть `Application` — **дороже**, чем прогон на железе, а выигрыш только для редких чистых функций.

**Когда имеет смысл вернуться.** GitHub CI без Nextion на runner; большая команда; много правок в IdMap/framer без доступа к панели.

| | |
|---|---|
| **Файлы** | `test/`, `[env:native]` в `platformio.ini` |
| **Сложность** | L |
| **Связано** | R402 (mock UART) |
| **Отложено** | 2026-06-20 — регрессия на панели достаточна; native tests не планируются |

---


**Было в backlog.** `MockByteStream` на ПК: скриптовать RX/TX, симулировать `Disconnected`, гонять `Application::update` без Nextion.

**Отложено вместо этого.** Регрессия — **on-target**: examples (example4/6) на реальной панели + `NEX_DEBUG` / `printStatusError` / UART-логи. Для текущего workflow mock не нужен.

**Когда вернуться.** CI без железа, scripted-тесты transparent (R301), или автоматическая проверка `enqueue`/`tryEnqueue` без прошивки платы.

| | |
|---|---|
| **Файлы** | `test/mock/`, `Interfaces/ibyte_stream.hpp` |
| **Сложность** | M |
| **Связано** | R401 (native), R301 |
| **Отложено** | 2026-06-18 — тесты на реальном железе с логами |
