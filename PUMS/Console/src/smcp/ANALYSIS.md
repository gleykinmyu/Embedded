# SMCP — что переделать

30 сентября 2026. Контракт кадра — `PROTOCOL.md`. Экран пульта — `src/Console/UI/TODO.md`. Здесь только дыры модели и шины.

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
