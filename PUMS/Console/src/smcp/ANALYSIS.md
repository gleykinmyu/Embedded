# SMCP — план

Сентябрь 2026. Идти сверху вниз. Экран пульта — `src/Console/UI/TODO.md`. Контракт кадра — `PROTOCOL.md`.

Канал один: консоль ↔ сервер сегмента по CAN1. Транспорт — `libs/smcp`. Northbound — `src/smcp/mech`. Механизмы по SMCP не ходят. `DriveMech` — локальный inventory сервера, не узел шины.

Плата: только CAN1 (`board.can`, PD0/PD1). `console_id` зашит как `1` в `Console/main.cpp`.

## Не трогать

- Второй линк внутри одного `IServer`. На CAN2 будет второй `Node`.
- CiA 402 внутрь SMCP. Чужой частотник — отдельный мастер на сервере.
- Подпись кадров. Поддельный Ack / HB / Telemetry — граница шины, не задача.
- Класс E и dual-axis Telemetry, пока нет сообщения, которому они нужны.
- `Group::Flag::Atomic`, `ErrorCode::Crc`, `Fault.detail` — полей нет поведения.
- UI поездки, пока пустой `DriveMech::setTarget`.



## Уже есть

- Wire, `Node`, `Session`, окно Ack = 1, Listen → Ready, IdConflict.
- North PDU вынесены из транспорта: Select, Block, GetTelemetry, SetTarget, Telemetry.
- Потеря HB снимает Select этого peer и шлёт Telemetry.
- Чужой Telemetry в зеркало не пишется. Второй сегмент без override `segment()` отбрасывается.
- Лимит Select один: `MServer::acceptSelect`, потолок `DriveMech::kMaxSelected` (3) и тестовая зона 7–9.
- У пульта один слот сессии. `MConsole::onStatus(Ready)` сам делает `start` к серверу `0x10`.
- Nack с ненулевым `detail` (кроме MechNotFound) запрашивает GetTelemetry.



## Дальше



### 1. Номер пульта

`begin(console_id)` уже есть. Нет места, откуда брать id после включения, и повторного `begin` после IdConflict с новым номером. Хранение на пульте — в плане UI. Здесь: прочитать id до `CanLink` / `begin`, не оставлять константу `1`.

### 2. Isolate и режим Спектакль в модели

`isolateGroup` глушит только Press клетки. `recall` выделяет изолированные оси. На сервере флага нет: это локальный запрет пульта.

- Проводить isolate в `trySelect` и `recall`, не только в цвете кнопки.
- Режим Спектакль тоже не должен слать Select. Запрет в `trySelect` / `recall` / `IConsole::select`. Страницы и меню — в плане UI.



### 3. Очередь Select

`CGroup::recall` ставит queued сразу после `setSelection`, не глядя, ушёл ли кадр. `Session::send` при закрытой сессии молча выходит, и поздний Ack другого Select активирует группу.

- Queued — только после успешной постановки в очередь.
- Любой Select вне recall сбрасывает queued.
- `IConsole::select` / `block` / `setTarget` без сессии не тихий return: пульту нужно событие, что кадр не поставлен.

`onPktIdGap` (дырка class A, только лог, сессию не рвать) — после этого, не вместо очереди.

### 4. Block и ширина маски

`acceptBlock` пускает любую консоль `0x01…0x0F`. `Action::Set` с пустой маской снимает Block со всего сегмента. Политику «кто имеет право» задать в leaf, когда на шине больше одного пульта.

Клетка в режиме Block сейчас шлёт этот сегментный Block. Жест на экране — в плане UI. Здесь оставить сегментный Block отдельной командой, не следствием нажатия клетки.

Маска GRUP — 32 бита, inventory пульта и сервера — 24. Биты 24…31 сервер ответит `MechNotFound`. Резать маску по `mechCapacity()` либо поднять inventory до 32.

### 5. Привод

- `DriveMech::setTarget`: Moving и позиция. Сейчас Ack и Telemetry уходят, ось стоит.
- `acceptSetTarget`: концевики и ход → `Limits`.
- `accel_mm_s2` на шину не кладётся и при разборе обнуляется. Либо класть в кадр, либо убрать поле из цели.
- `resetFault()` пустой с обеих сторон, MsgId нет. Кнопка сброса на клетке ждёт это сообщение.
- Телеметрия движения — по Δposition / rate-limit на сервере. Базовый `IServer` шлёт снимок при смене select, не при ходе.



### 6. CAN2

Второй `Node` на сервере: CAN1 — сессии консолей, CAN2 — сессии плат. Свой PDU привода, не расширение `smcp/message.hpp` и не заголовок внутри payload. `Select` / `Block` / holder на CAN2 не тащить. `DriveMech` мапит плату на логический `IMech`. Пульт по-прежнему видит «ось N на `server_id`».

Открыть CAN2 на F407. Сейчас в `board` только CAN1.

### 7. Второй сегмент

Когда появится второй сервер: перекрыть `IConsole::segment()`, не писать чужой `mech_id` в primary-банк. `serverIdForGlobalMech` уже есть и никем не вызывается.