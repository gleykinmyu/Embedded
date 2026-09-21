# SMCP MVP — протокол (консоль ↔ сервер сегмента)

Канал: CAN extended ID.  
Транспорт: `smcp/transport/message.hpp` (Ack, Nack, Heartbeat, конверт).  
Northbound PDU: `smcp/Console/console_message.hpp` (Select, Block, GetTelemetry, SetTarget, Telemetry).

```text
CAN ID (29 bit, IDE=1):
  msg_id[28:22] | dst[21:14] | src[13:6] | pkt_id[5:0]
data[0..7]: непрозрачный payload (без pkt_id).
Class C/D/E: pkt_id = 0. Младший msg_id выигрывает арбитраж.
```

Нумерация (QoS): Ack/Nack `0x01…` → команды `0x10…` → HB `0x30` → Telemetry `0x40…`.

Консоли `0x01…0x0F`, серверы `0x10…0xEF`, broadcast `0xFF`. Id `0` не занимать.

Лимит Select на сегменте — leaf в `acceptSelect`: слияние (чужие Selected ∪ маска консоли) → **SelectLimit**.

---

## Роли сообщений

| Msg | Кто → кому | Смысл |
|-----|------------|--------|
| **Select** | Console → Server | Выделение осей (Action Add / Remove / Set) |
| **Block** | Console → Server | Сегментный Blocked (Action Add / Remove / Set) |
| **GetTelemetry** | Console → Server | Запрос снимков осей по маске (без смены состояния) |
| **Ack / Nack** | Server → Console | Принятие / отказ **запроса** (`pkt_id` = запроса; Nack: `code` + `detail`) |
| **Telemetry** | Server → (обычно broadcast) | Снимок оси после **изменения** состояния / движения / GetTelemetry |
| **Heartbeat** | Console → Server (первый ping); далее ping/pong | Линк + «мягкая» регистрация консоли |
| **SetTarget** | Console → Server | Цель движения (класс A) |

---

## Select → Ack + Telemetry

```text
Console:  Select(mask, action, pkt_id=N)
Server:   1) Проверка (без изменений): маска валидна / свободна или наша /
             Ready / не Blocked / `acceptSelect` — иначе Nack
             (Busy / Safety / NotReady / SelectLimit / MechNotFound)
          2) Commit: take и drop в одном проходе, затем Ack + Telemetry по сменившимся
```

- Deselect снимает **только своё**; бит чужого holder → Nack Busy. Set с bit=0 чужого не трогает.
- В Telemetry есть **`holder_id`** (кто держит Select). Смена `0 ↔ console` = изменение → Telemetry обязателен.
- Повторный Select без изменений → **Ack**, Telemetry можно не слать.
- Массовый Set на 32 оси с 32 изменениями → 1 Ack + 32 Telemetry (редко; ок для CAN).
- Консоль по Nack с `detail=mech_id` запрашивает **GetTelemetry** по этой оси (зеркало только из Telemetry).

Telemetry при **движении** — отдельно: по Δposition / rate-limit на стороне сервера (наследник / приводной слой). Базовый `IServer` шлёт Telemetry при смене select-состояния.

**Не** трактовать Telemetry как RPC-ответ на Select: ответ на Select — Ack/Nack.

---

## GetTelemetry → Ack + Telemetry

```text
Console:  GetTelemetry(mask, pkt_id=N)
Server:   1) Оси из mask существуют (иначе Nack MechNotFound)
             Пустая mask = все оси inventory сегмента
          2) Ack + Telemetry (D) по каждой запрошенной оси — снимок без commit
```

После Online консоль обычно запрашивает маску своих слотов (зеркало Ready/holder до первого Select).

---

## Block → Ack + Telemetry

```text
Console:  Block(mask, action, pkt_id=N)
Server:   1) MechNotFound / `acceptBlock` — иначе Nack
          2) Commit: Status::Blocked take/drop, снятие Select при block (IMech::block)
             → Ack + Telemetry по сменившимся
```

- **Add / Remove** — дельта по маске; **Set** — абсолютная маска blocked сегмента.
- GRUP на пульте ≠ сегментный Block (локальный UI).
- Кто имеет право слать Block — leaf `acceptBlock` (пока Ok всем консолям).

---

## SetTarget → Ack + Telemetry

```text
Console:  SetTarget(mech_id, target, pkt_id=N)
Server:   1) MechNotFound / не наш Select → Busy / Blocked → Safety /
             не Ready → NotReady / `acceptSetTarget` (Limits) — иначе Nack
          2) IMech::setTarget → Ack + Telemetry
```

Движение (Moving / Δposition) — в leaf/приводе; базовый сервер шлёт снимок после accept.

---

## Heartbeat (линк + регистрация)

Open/Close на wire нет. Локально: `Session::Status` / `linkUp()`.

В Node нет ролей «консоль/сервер». Сессия: `Idle` / `Connecting` / `Awaiting` / `Open`
(`Awaiting` = первый ping, ещё не up; keep-alive из `Open` — только `_hb.isWaiting()`).
`_master` (`start(peer)`) — кто шлёт keep-alive HB; без `start` — только pong + сторож тишины.
Кто master, решает приложение (консоль: `begin(server_id)`; сервер: слоты без `start`).

| | Master (`_master` / `start`) | Slave |
|--|---------|---------|
| Connecting | `ping` → `Awaiting` | — |
| Awaiting + T | retry / `hbLost` | — |
| Open + T (не waiting) | `ping()` keep-alive (остаёмся Open) | `miss` |
| Open + T (`_hb.waiting`) | retry / `hbLost` | `miss` / `hbLost` |
| RX HB | `_hb.clear`+`start(...,false)`, `Open` | то же + pong если не ждали |
| misses ≥ N | `onHbLost` → `close` | `onHbLost` → `close` |

`isOpen()` = **только** `Open` (Awaiting ≠ open).  

```text
Master:  start → Connecting → ping → Awaiting → RX → Open
         … keep-alive: Open + _hb.waiting, без смены Status …
```

Один `MsTimer` + счётчик `_hb_misses`.  
«Сессия открыта» = `Session::isOpen()`.

API консоли: `setConsoleId` / `begin(server)` / `serverId` / `update` / `linkUp` / `onPhase`.
Сервер: `sessionByPeer` / `SessionBank<SessionConsole>`, без единого `consoleId`.
`Node::getStatus`/`clearError`; часы — в ctor `Node` (stall enqueue).

RX любого кадра с `src_id == мой id` → **`Node::Status::IdConflict`**:
не demux RX и не TX (`send` / `sendWire` / drain), пока `clearError()`.
`Session::isOpen()` при этом может остаться true (линк peer ≠ статус узла):
сессии не рвём в transport — `onStatus(IdConflict)` наверху (close / UI / смена id).

`Node::send` при полной TX-очереди крутит `update()` до `SMCP_TX_STALL_MS` (как nex enqueue).

### Link / Session / Node

| | |
|--|--|
| **ILink / CanLink** | среда ↔ Packet; один shot `send`/`receive` |
| **Node** | bus TX + registry Session*; pump RX/`acceptRx`/session/`onPacket` + RR Session TX + Node::_tx; **IdConflict** |
| **Session** | peer, `Status`, pkt_id, **своя TX-очередь**; `open`/`close` чистят очередь |

`IConsole` / `IServer` наследуют **Node**, держат registry `Session*`;
объекты: `Console` — primary `Session` (+ `MaxSessions+1` слотов);
`MServer` — `SessionBank<SessionConsole>`. Leaf может добавить ещё Session в свободные слоты.

### Конфликт одинаковых `console_id`

Сервер **не различает** двух консолей с одним ID — для него это один `src_id`.
Отдельный Register / участие сервера в разруливании дубля **не нужны**.

Правило на консоли (first-wins): `IConsole::Phase` + `onPhase`
(`Idle` / `Listen` / `Connecting` / `Online` / `Fault`):
1. `begin(server)` → **Listen** ~`Heartbeat::kTimeoutMs`.
2. Кадр с `src_id == мой id` → **Fault** (`Node::IdConflict`).
3. Listen OK → **Connecting** → **Online**. HB lost → снова **Connecting**.
4. `setConsoleId` → снова `begin(server)` (новый Listen).
   TxFull / Nack / LinkError — `SMCP_NODE` / `SMCP_SESS` / `SMCP_CONS` (см. `debug.hpp`), не Phase.
   Повтор после Fault: `setConsoleId` и/или `begin()` (`clearError` внутри).

Тот же пункт у сервера — для **своего** id.

---

## Очереди

- Очередь **кадров** — в `ICAN` (драйвер / `MockCan`).
- Очередь **bus / broadcast** `(msg_id, data, dlc, dst, pkt_id)` — `Node::_tx` (`Node::send`, класс **D**).
  `TxQueue::isFull` после stall → `onTxFull(nullptr)`.
- Очередь **session unicast** — снаружи одна (`transmit` / `isTxFull`);
  внутри `Session::_tx_req` (A) + `_tx_ctrl` (HB/Ack/Nack).
  Full любой → `isTxFull()`; edge → `onTxFull(session)`.
- Drain: один RR на `sessionCapacity + 1` (слоты сессий + bus), **по одному кадру** за заход.
- Окно Ack = **1**: class A после wire — голова `_tx_req` + `_ack` (не drop до reply);
  `_tx_ctrl` / `_tx_req` — RR в `peekTx` (после успешного wire).
  Retry головы req по timeout; Ack/Nack / `Timeout` → hook (`onAck`/`onNack`, `req` жив)
  → затем drop головы `_tx_req`.
  Diag (default empty): `onPktIdMismatch(expected, got)` (reply ≠ pending). Сессию не рвём.
- Dual-axis Telemetry — **не** в MVP.

---

## Правила расширения (обязательные)

Цель: новый `MsgId` добавляется по чеклисту, без споров «куда метод / куда очередь».

### 1. Класс сообщения (выбрать ровно один)

| Класс | Признаки | Слой TX | `pkt_id` | Очередь Session |
|-------|----------|---------|----------|-----------------|
| **A. Session request** | unicast → peer, ждёт Ack/Nack | `Session` | `Packet.pkt_id`, TX назначает | `_tx_req` (retry/окно) |
| **B. Session reply** | Ack/Nack (или аналог) на request | `Session::sendAck/Nack` | echo → `Packet.pkt_id` | `_tx_ctrl` |
| **C. Session control** | линк/сессия без RPC (сейчас HB) | внутри `Session` | нет | `_tx_ctrl` |
| **D. Broadcast / announce** | `dst=0xFF`, без диалога | `Node::send(body, kBroadcastId)` | нет | нет (`Node::_tx`) |
| **E. Unicast notify** | unicast, **без** Ack (событие) | `Session` / `Node::send` | нет | `_tx_ctrl` / `Node::_tx` |

Примеры: Select/Block/SetTarget → **A**; Ack/Nack → **B**; Heartbeat → **C**; Telemetry → **D**.

**pkt_id** — признак сессии (`Packet.pkt_id`), не поле body. На wire — в CAN ID (6 бит).  
Class C/D/E: `pkt_id=0`. Body/`data[]` без `pkt_id`.

### 2. Wire / codec

1. Транспортные PDU — `message.hpp`. Прикладные — свой файл (`console_message.hpp`, не `message.hpp`).
2. Конверт: `Packet { src, dst, pkt_id, Message { id, data[8], dlc } }`. Мирового `variant` нет.
3. `Packet::pack` / `Packet::unpack` не demux'ят приложение: неизвестный `msg_id` — валидный opaque payload.
4. `struct` PDU: `kId`, `kNeedsAck`, `pack` / `unpack` (payload без pkt_id).
5. Класс **A**: `T::kNeedsAck == true` — окно Session. **B/C/D/E** — false.
6. Приоритет = номер `msg_id` (без отдельного `prio`). Не класть opcode в data.
7. Demux: `helpers::take<T>(pkt, fn)` в наследнике Session (unicast A/E) или Node (class D).

### 3. Session API (не плодить метод на каждый MsgId)

- Класс **A**: `Session::send(Select{})` / шаблон `send(T)` (`pkt_id` внутри Session).
- Класс **B**: `sendAck(req_pkt_id)` / `sendNack(req_pkt_id, code, detail=0xFF)`.
  Nack DLC=2: `code | detail` (`detail` обычно `mech_id`, `kNackDetailNone` = нет). Ack DLC=0.
- Класс **C**: логика внутри Session (`tick` / `onHeartbeat`).
- Фасады `IConsole::select` / `setTarget` — над `send`, не транспорт.

Виртуальный `IMessage` — не правило. Новый PDU = struct + `take<T>` в нужном листе.

### 4. Узел (IConsole / IServer)

- RX: `Node::update` → `acceptRx` → `sessionByPeer` / `Session::onPacket` → иначе `Node::onPacket`.
- Новые **A** на сервере: `SessionConsole::onPacket` → `take<Select/…>` + Ack/Nack; снимок — Telemetry (**D**).
- Новые **D**: `Node::send(Telemetry{}, kBroadcastId)` и разбор в **наследнике Node** (`IConsole::onPacket`). Не через Session.
- Политика отказа Select (`acceptSelect`) / Block (`acceptBlock`) / SetTarget (Limits) — в наследниках.

### 5. Чеклист «добавил MsgId»

- [ ] Класс A–E выбран и записан в таблицу «Роли сообщений»
- [ ] `kId` / `kNeedsAck` / pack/unpack согласованы с классом (PDU не в `message.hpp`, если не транспорт)
- [ ] TX путь: Session queue (**A**), reply (**B**), internal (**C**), broadcast Node (**D**), notify (**E**)
- [ ] RX путь: наследник Session (unicast) или наследник Node (broadcast)
- [ ] `PROTOCOL.md` обновлён (роль + кто → кому)

---

## Два «чипа»

Консоль и сервер — независимые узлы (`poll` каждый свой `Link`).  
На одном MCU отладка: два `MockCan` + `connect()`.
