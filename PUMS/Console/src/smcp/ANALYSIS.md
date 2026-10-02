# SMCP — что переделать

30 сентября 2026. Контракт кадра — `PROTOCOL.md`. Экран пульта — `src/Console/UI/TODO.md`. Здесь только дыры модели и шины.

## Файл / FIO (решения + TODO)

Принято при разборе multi-server / SectionPool (A1–A4). Реализацию не делать в этом же проходе — сначала закрыть остальные дыры списка.

### Решения

- **A1.** Один live `Show` + staging `Show` (как сейчас у FIO). Тип файла = набор секций (SERV/GRUP/PRST/SETT…). Load/save — **чекбоксы по секциям**; пустые = no-op. «Чистый патч» / «чистое шоу» — какие секции выбраны/есть, не два класса Show.
- **A2.** Load: GRUP/PRST `seg_id` ∉ SERV → **предупреждение**, файл принимаем. Слоты на отсутствующий seg — тот же UI, но recall/record **disarm** до правки патча + save. Данные из файла не чистить.
- **A3.** Пока две копии Show (live + incoming). Overflow ёмкости → MsgBox «Загрузить частично?» (факты: секция, headers/slots found vs max); Yes = truncate, No = секцию не трогать. Потолки `{tag, max_count}` — таблица в прошивке (числа позже).
- **A4.** SD = полный файл. W25 = LKG/boot-зеркало: **два последних 4K-сектора A/B** (ping-pong, seq + magic/CRC). После успешного save на SD — обновить LKG. Чтение сектора ~4 ms @ 8 MHz; ресурс ≥100k P/E/сектор.
- **A5.** Обратная совместимость / миграция старого GRUP — **не делаем**, пока нет прода и полевых файлов. Пишем только новый формат; `Header.version` не разветвляем.
- **A6.** `markEdited` / dirty — **на секцию (или пул)**, не один флаг на весь Show. UI `*` и save смотрят маску dirty; save = чекбоксы ∩ dirty (или явный override чекбоксами). Load clear dirty у принятых секций.

### TODO — переформатировать `sf::Fio` / W25 mirror

Когда дойдём до реализации (после закрытия дизайн-дыр A…):

1. Load/save по маске секций (чекбоксы), не «весь файл целиком».
2. Accept с предупреждениями A2 + disarm слотов; revalidate после save патча.
3. Overflow-диалог A3; без тихого truncate.
4. `W25qShowFile`: A/B два сектора, commit после program; `restore()` читает активный слот.
5. Зеркало W25 после успешного SD-save (не вместо SD).
6. Per-section / per-pool `edited` (A6); UI `*` и save по маске dirty.

## Адресация / сегменты (решения)

- **B1.** На пульте **нет** `mech[id]` через `storage(server_id)` / ObjRegistry. Есть `CMechBank` — слоты, которые **заполняются по SERV** (субсекции механизмов). Слот = оперативка (live) + ссылка на config в SERV. `storage(server_id)` у консоли убираем; lookup — банк слотов + правила SERV. (На сервере свой `storage()` для приводов — отдельно.)
- **B2.** Gaps `mech_id` **разрешены**, id не уплотняем. Нет в SERV → пустая клетка; бит на отсутствующий id → MechNotFound / вырезать при record.

### 1. Номер пульта

`begin(console_id)` есть. Id после включения неоткуда взять, повторного `begin` после IdConflict с новым номером нет.

Сейчас константа `1`: `CanLink(board.can, 1u)` и `console.begin(kConsoleIdMin)` в `Console/main.cpp`, то же в `main_test.cpp`.

Прочитать id до `CanLink` / `begin`. Хранение и страница настроек — `UI/TODO.md` §2. После смены id leaf снова вызывает `start(server)`.

### 2. Isolate и режим Спектакль

`isolateGroup` — SETT шоуфайла, на сервере флага нет. Сейчас глушит только Press клетки. `CGroup::recall` и `CGMech::trySelect` isolate не смотрят.

`MConsole::Mode::Show` гасит меню. `onCellPress` / `recall` mode не проверяют — Select уходит.

- isolate — в `trySelect` и `recall`;
- Спектакль — запрет Select в `trySelect` / `recall` / `IConsole::select`.

Guard страниц от HMI `page` по UART — план UI §5.

### 3. Block и ширина маски

Клетка в режиме Block шлёт сегментный PDU `Block`. Флаг Blocked в GRUP — локальный, сервер его не знает.

`acceptBlock` в базе — Ok любой консоли `0x01…0x0F`. `Action::Set` с пустой маской снимает Block со всего сегмента. Кто имеет право слать Block — задать в leaf, когда пультов больше одного.

Жест клетки и группы — `UI/TODO.md` §6. Сегментный Block не делать следствием нажатия клетки.

Маска `Selection` — 32 бита. Inventory пульта и сервера — 24. Биты 24…31 сервер ответит `MechNotFound`. Резать маску по `mechCapacity()` либо поднять inventory до 32.

### 4. Привод

`DriveMech::setTarget` пустой: Ack и Telemetry уходят, ось стоит.

- Moving и позиция, либо отказ до Ack.
- `acceptSetTarget`: концевики и ход → `Limits`. Сейчас Ok всем.
- `accel_mm_s2` на шину не кладётся и при разборе обнуляется. Класть в кадр либо убрать из цели.
- `resetFault()` пустой, MsgId нет. Кнопку сброса не включать раньше PDU.
- Телеметрия хода — Δposition / rate-limit в тике привода, `pushTelemetry`. Базовый `IServer` шлёт снимок при смене select / block / SetTarget / GetTelemetry / HbLost, не при движении.

Пока привод пустой — поездку на экране не делать (`UI/TODO.md`).

### 5. CAN2

Второй `Node` на сервере: CAN1 — консоли, CAN2 — платы. Свой PDU привода, не расширение транспортного `message.hpp`. `Select` / `Block` / holder на CAN2 не тащить. `DriveMech` мапит плату на `IMech`. Пульт видит «ось N на `server_id`».

CAN2 на F407 закрыт: в `board` только CAN1. Один `Node` на два `ILink` не делать.

### 6. Второй сегмент

Второй сервер: в `MConsole::storage` вернуть второй банк. `serverIdForGlobalMech` / `mechIdForGlobalMech` нигде не вызываются.

Второй слот сессии — `GroupConsole<24>` сменить на `MaxSessions > 1` и расширить `SessionBank` в `MConsole`. Свободного слота под второго peer нет.
