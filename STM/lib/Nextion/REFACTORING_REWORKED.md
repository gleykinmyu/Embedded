# Nextion — переработанный backlog

Пункты с **уточнённым scope** или **вынесенной спецификацией**. Активный backlog наряду с [smartApp/IdMap.md](smartApp/IdMap.md) и [TRANSPARENT_PROTOCOL.md](TRANSPARENT_PROTOCOL.md). **Отложено:** [REFACTORING_DEFERRED.md](REFACTORING_DEFERRED.md).

**Transparent Data Mode (NIS §1.16):** весь backlog и code TODO — [TRANSPARENT_PROTOCOL.md](TRANSPARENT_PROTOCOL.md) (R301, R302, R215).

**Отложено без переработки:** [REFACTORING_DEFERRED.md](REFACTORING_DEFERRED.md).

**Легенда:** `[ ]` — к реализации; в заголовке — кратко «было → стало».

## Фаза 3 — Components / attr API

**Transparent (`addt`, facades `*_t`):** → [TRANSPARENT_PROTOCOL.md](TRANSPARENT_PROTOCOL.md) (NEX-R215, R301, R302).

---

### [ ] NEX-R217 — Mirror attr/SysVar после успешного enqueue

**Проблема.** `attr::Num::operator=` и `SysVar::operator=` пишут зеркало `_val` **до** `enqueueTransaction`. Если enqueue fail (QueueFull после spin, Session fault) — MCU и панель расходятся.

**Как сейчас.** Optimistic update на assign; `applyResponse` / `copy_string_mirror` — только с панели на get.

**Цель (варианты — выбрать при реализации).**
1. **Pessimistic:** обновлять `_val` только после Success status (или после `tryEnqueue` → `true`).
2. **Rollback:** сохранять old value, откат в `onStatus` по route транзакции.
3. Единая политика для `attr::Num`, `attr::String`, `SysVar`.

**Критерий готовности.** Unit/integration: assign при forced QueueFull → зеркало = старое значение (или откат после fail).

| | |
|---|---|
| **Файлы** | `comp/nexAttributes.hpp`, `app/nexSysVars.hpp`, `app/nexApplication.cpp` |
| **Сложность** | M |
| **Зависит от** | NEX-R101 ✓ (`tryEnqueue`) |
| **Перенесено** | 2026-06-18 — spec из REFACTORING_REMAINING |

---

### [x] NEX-R218 — ExComponents: полный рефакторинг `DataFile` / `DataRecord` / `FileBrowser`

**Сделано (2026-06-25).** База `DataFile` — только общие attr; `dir`/`val`/`dis`/`bco2`/`pco2` в листьях. Исправлено: `setCharSpacing` → `spax` (было ошибочно `dis`); `DataRecord`: `format`/`dir` как строки, `setColumnWidths`/`setColumnHeaders`, `val` RO; `FileBrowser`: `dir` — папка, `setScrollStep` → `dis`, `setSelBgColor`/`setSelColor`. Ручная проверка на HMI — по желанию.

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

## Пометки

- Проверить изменения **NEX-R103** на железе: RX fault / retry (`DataError`, `OverFlowRX`), status вне активной транзакции `(0,0)`, отсутствие регрессии `example4` / `example6`.
