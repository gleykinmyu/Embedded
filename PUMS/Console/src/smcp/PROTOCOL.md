# SMCP — транспорт (Link / Node / Session)

Только слой до **Node** и **Session** включительно: кадр, адреса, классы сообщений, Heartbeat, очереди, правила расширения.  
Всё, что выше (Select / Block / SetTarget / Telemetry…, `IConsole` / `IServer`, отношения консоль ↔ сервер) — [`SYSTEM.md`](SYSTEM.md). План — [`IMPLEMENTATION.md`](IMPLEMENTATION.md).

Канал: CAN extended ID.  
Транспортные PDU: `smcp/message.hpp` (Ack, Nack, Fault, Heartbeat, Get/Set/DataBegin/Data, конверт; lib `libs/smcp`).  
Прикладные PDU — в своём слое (northbound: `smcp/mech/message.hpp`, см. `SYSTEM.md`).

```text
CAN ID (29 bit, IDE=1):
  msg_id[28:22] | dst[21:14] | src[13:6] | txn_id[5:0]
data[0..7]: непрозрачный payload (без txn_id).
Class C/D/E: txn_id = 0. Младший msg_id выигрывает арбитраж.
```

Нумерация (QoS, меньший id выигрывает): Ack/Nack/Fault `0x01…` → команды `0x10…0x2F` → HB `0x30` → config `0x31…0x3A` → Telemetry `0x40…`.  
`0x30` — транспортный Heartbeat, прикладной слой его не занимает.

Адреса узлов: консоли `0x01…0x0F`, серверы `0x10…0xEF`, broadcast `0xFF`. Id `0` не занимать.

---

## Транспортные сообщения

| Msg | Кто → кому | Смысл |
|-----|------------|--------|
| **Ack / Nack** | peer → peer | Принятие / отказ **запроса** класса A (`txn_id` = запроса; Nack: `error` + `detail` u32) |
| **Fault** | узел → broadcast | Отказ / уход на шине; DLC=1…8: `error` + `detail[]`; `txn_id=0`. `IdConflict` — пиры только лог; **иной** Fault → у пира `close(PeerDown)` |
| **Heartbeat** | master → slave (ping), далее ping/pong | Линк + «мягкая» регистрация peer |

Коды `error` / `detail` в Nack — opaque для транспорта; значения задаёт прикладной слой.

---

## Heartbeat (линк + регистрация)

Open/Close на wire нет. Локально: `Session::isOpen()`.

В Node нет ролей «консоль/сервер». Сессия: `Idle` / `Connecting` / `Awaiting` / `Open`
(`Awaiting` = первый ping, ещё не up; keep-alive из `Open` — только `_hb.isWaiting()`).
`_master` (`start(peer)`) — кто шлёт keep-alive HB; без `start` — только pong + сторож тишины.
Кто master, решает приложение (см. `SYSTEM.md`).

| | Master (`_master` / `start`) | Slave |
|--|---------|---------|
| Connecting | `ping` → `Awaiting` | — |
| Awaiting + T | retry / `close(HbLost)` | — |
| Open + T (не waiting) | `ping()` keep-alive (остаёмся Open) | `miss` |
| Open + T (`_hb.waiting`) | retry / `close(HbLost)` | `miss` / `close(HbLost)` |
| RX HB | `_hb.clear`+`start(...,false)`, `Open` | то же + pong если не ждали |
| misses ≥ N | `close(HbLost)` → `onFault` + Idle | `close(HbLost)` → `onFault` + Idle |

`isOpen()` = **только** `Open` (Awaiting ≠ open).  

```text
Master:  start → Connecting → ping → Awaiting → RX → Open
         … keep-alive: Open + _hb.waiting, без смены Status …
```

Один `MsTimer` + счётчик `_hb_misses`.  
«Сессия открыта» = `Session::isOpen()`.

`Node::begin` / `end` / `getStatus` / `clearError`; часы — в ctor `Node` (stall enqueue).
`Node::Status`: Idle → Listen (~`Heartbeat::kTimeoutMs` в `update`) → Ready; sticky LinkError / RegisterFailed / IdConflict.

RX кадра с `src_id == мой id` (кроме **Fault IdConflict**) → **`Node::enterIdConflict`**:
1. Сразу broadcast **Fault** (`error=IdConflict`, `detail` = короткое имя узла `Node::name()`, `dst=FF`, `txn_id=0`) — пока TX ещё жив.
2. `closeAllSessions(IdConflict)`, drain, **`onFault(nullptr)`**, sticky **`Node::Status::IdConflict`**. TX/RX стоп до `clearError()`.

Типичный сценарий: новичок / смена id в **Listen** слышит живой трафик с этим id → сам уходит в IdConflict и шлёт Fault. Ready-инкумбент чужой **Fault IdConflict** с «своим» src **не** трактует как конфликт (иначе суицид + HbLost у пиров). Иной Fault / любой другой PDU с чужим src — по-прежнему конфликт.
Чужой **Fault IdConflict**: сессию у пиров не закрываем (лог); мёртвый peer → `HbLost`.
После смены своего id наследник заново `start()` / ждёт HB; Node слоты не конфигурирует.

`Node::send` при полной TX-очереди крутит `update()` до `SMCP_TX_STALL_MS` (как nex enqueue).

### Link / Session / Node

| | |
|--|--|
| **ILink / CanLink** | среда ↔ Packet; один shot `send`/`receive` |
| **Node** | `begin`/`end` (Idle/Listen/Ready), bus TX + registry Session*; pump RX/`acceptRx`/Fault/`session`/`onPacket` + RR Session TX + Node::_tx; **IdConflict** (Fault + close) |
| **Session** | peer, `Status`, txn_id, **своя TX-очередь**; `open`/`close` чистят очередь |

Объекты сессий держит наследник Node (`SessionBank<…>`, `MaxSessions` слотов, без запасного); Node хранит registry `Session*`. Наследник может добавить ещё Session в свободные слоты.

### Конфликт одинаковых id

Peer **не различает** двух узлов с одним id — для него это один `src_id`.
Отдельный Register / участие peer в разруливании дубля **не нужны**.

Правило first-wins: `Node::Status` + `onStatus`
(`Idle` / `Listen` / `Ready`; отказы sticky):
1. `begin(id)` → **Listen** ~`Heartbeat::kTimeoutMs` (свой id на шине, не peer). Любой узел.
2. Кадр с `src_id == мой id` → broadcast Fault IdConflict + close сессий + Status **IdConflict**.
3. Listen OK → **Ready**. Дальше приложение `start(peer)` (master HB). `end()` → `closeAllSessions(None)` → Idle.
4. HB lost: Status остаётся Ready, `close(HbLost)` → `onFault`; снова `start()` — решение наследника. `stop` — `close(None)` без `onFault`.
5. Смена своего id — снова `begin(id)` (новый Listen; живые сессии — `closeAllSessions(None)`).
   TxFull / Nack / LinkError — `SMCP_NODE` / `SMCP_SESS` / `SMCP_CONS` (см. `debug.hpp`), не смена Idle/Listen/Ready.
   Повтор после IdConflict: `begin(id)` (`clearError` внутри).

---

## Очереди

- Очередь **кадров** — в `ICAN` (драйвер / `MockCan`).
- Очередь **bus / broadcast** `(msg_id, data, dlc, dst, txn_id)` — `Node::_tx` (`Node::send`, класс **D**).
  `TxQueue::isFull` после stall → `onTxFull(nullptr)`.
- Очередь **session unicast** — снаружи `transmit`;
  внутри `Session::_tx_req` (A) + `_tx_ctrl` (HB/Ack/Nack).
  Full → `onTxFull(session)`.
- Drain: один RR на `sessionCapacity + 1` (слоты сессий + bus), **по одному кадру** за заход.
- Окно Ack = **1**: class A после wire — голова `_tx_req` + `_ack` (не drop до reply);
  `_tx_ctrl` / `_tx_req` — RR в `peekTx` (после успешного wire).
  Retry головы req по timeout; Ack/Nack / `Timeout` → hook (`onAck`/`onNack`, `req` и `reply` живы)
  → затем drop головы `_tx_req`.
  Чужой txn на окне A — не наш пакет, дальше по цепочке (TaggedOps).
  `onPackFailed(session, msg_id)` — PDU не pack, кадр не в очередь (`nullptr` — bus).
  Свой txn_id, но Ack/Nack не unpack — окно не закрываем, дальше retry / Timeout.

---

## Правила расширения (обязательные)

Цель: новый `MsgId` добавляется по чеклисту, без споров «куда метод / куда очередь».

### 1. Класс сообщения (выбрать ровно один)

| Класс | Признаки | Слой TX | `txn_id` | Очередь Session |
|-------|----------|---------|----------|-----------------|
| **A. Session request** | unicast → peer, ждёт Ack/Nack | `Session::request` | `Packet.txn_id`, TX назначает | ReplyWnd / `_tx_req` |
| **B. Session reply** | Ack/Nack (или аналог) на request | `Session::sendAck/Nack` | echo → `Packet.txn_id` | `_tx_ctrl` |
| **C. Session control** | линк/сессия без RPC (сейчас HB) | внутри `Session` | нет | `_tx_ctrl` |
| **D. Broadcast / announce** | `dst=0xFF`, без диалога | `Node::send(body, Packet::kBroadcastId)` | нет | нет (`Node::_tx`) |
| **E. Unicast notify** | unicast, **без** Ack (событие) | `Session` / `Node::send` | нет | `_tx_ctrl` / `Node::_tx` |

Транспорт: Ack/Nack → **B**; Heartbeat → **C**; Fault → **D**; short RPC (Select…) → **A** (`request`); Get/Set → **TaggedOps** (не ReplyWnd); DataBegin / Data → MultiByte (`txn_id` эхо).  
Классы прикладных MsgId — каталог в `SYSTEM.md`.

### Inline vs multi (`kInlineMax` = 7)

Один порог на обе стороны:
- **Set**: длина с DLC (отдельного поля нет). DLC=1 (`len=0`) → multi (DataBegin→Data*); DLC=2…8 → inline `len=1…7`. Пустой `set`/`send` (`size=0`) и **DataBegin(`size=0`)** запрещены.
- **Ответ Get**: `len ≤ 7` → один **Data** (seq=0), его разбирает **TaggedOps**; иначе DataBegin→Data* (`size≥1`) — **MultiByte**. Иначе **Nack**. Клиент после тела на wire **не** шлёт Ack/Nack.

API:
- Short RPC (ждёт Ack|Nack): `Session::request(T)` → **ReplyWnd** (`_tx_req` + окно).
- Без окна: `Session::send` / `send(T)` → `_tx_ctrl` (HB, Ack/Nack, тело, notify).
- Get/Set: `Session::set` / `get` / `send(tag,IBuf,txn)` → **TaggedOps** (свои таймеры; opener+тело в ctrl).
- Set RX — `onSetBuffer` → `onSet`; Get client — `onReply`; server Get — `onGet` → `send(tag,IBuf,txn)`.
- Длинное тело — **MultiByte** (DataBegin→Data*, не знает Get/Set).

### Тело txn (DataBegin / Data)

Корреляция — `Packet.txn_id` (не поле body). End-кадра нет.

| Id | PDU | Тело |
|----|-----|------|
| 0x33 | **DataBegin** | `tag \| size:u16 LE \| crc32:u32 LE` (DLC=7), **size≥1** |
| 0x34 | **Data** | `seq \| data≤7` (DLC=1…8) |

- Конец multi: `offset == size` + CRC у RX (или TX дослал все куски).
- Ошибки тела **Set (RX)**: `onSet` + `Nack(Body, detail=MultiByte::Error)`.
- Ошибки тела **Get (клиент)**: `onReply` (в т.ч. `Timeout`); wire Nack opener → `onNack` + `onReply(Abort)`.
- Set-клиент после тела ждёт Ack|Nack (таймер TaggedOps, не ReplyWnd); локальный abort → `onNack(kTimeout)`.
- Транспортный `Nack.error`: ≤ `Max=0x0F`; прикладной — **выше Max** (`ErrorCode` с `0x10…`).
- Ожидание DataBegin и inter-frame multi: `MultiByte::tick`. Короткий Data своего Get и Ack-timeout Set-клиента — TaggedOps.
- **Один** tagged job на Session (`TaggedOps::isBusy`); short Get-reply (`sendBody` inline) state не занимает.

**txn_id** — id транзакции сессии (`Packet.txn_id`), не поле body. На wire — в CAN ID (6 бит).  
`0` зарезервирован (вне txn / ctrl / broadcast). Opener class A: `allocTxn()` → `1…kTxnIdMax` (63). Class B: echo. `_txn_tx` — последний выделенный.

### 2. Wire / codec

1. Транспортные PDU — `smcp/message.hpp`. Прикладные — свой файл в слое (`smcp/mech/message.hpp`).
   Nack несёт opaque `error`/`detail`; коды прикладного слоя — в его `message.hpp`.
   Локальный abort Ack — `Nack::kTimeout` (не с шины).
   Fault — DLC=1…8: `error` + `detail[0…7]` (не Nack); `Fault::kIdConflict` — дубль `src`; detail = ASCII-имя (≤7).
   Свой IdConflict: Node шлёт Fault через `send` (ещё не sticky), drain, затем close сессий.
2. Конверт: `Packet { src, dst, txn_id, Message { id, data[8], dlc } }`. Мирового `variant` нет.
3. `Packet::pack` / `Packet::unpack` не demux'ят приложение: неизвестный `msg_id` — валидный opaque payload.
4. `struct` PDU: `kId`; `pack`/`unpack` Message.
5. Класс **A**: `Session::request(T)` → ReplyWnd (`_tx_req` + окно + `allocTxn`).
6. Приоритет = номер `msg_id` (без отдельного `prio`). Не класть opcode в data.
7. Demux: `SMCP_IF_MSG(Type) { … body … }` в `onPacket(pkt)` (имя `pkt` в скоупе).
   Цепочка `else SMCP_IF_MSG`. `body` свой в каждой ветке.

### 3. Session API (не плодить метод на каждый MsgId)

- Класс **A** (short RPC): `Session::request(T)` → ReplyWnd; ответ → `onAck` / `onNack`.
- Без окна: `Session::send` / `send(T)` → `_tx_ctrl`.
- Get/Set: `set` / `get` / `send(tag, buf, txn)` → TaggedOps (+ MultiByte на длинное тело).
- Класс **B**: `sendAck(txn_id)` / `sendNack(txn_id, error, detail=0)`.
  Nack DLC=5: `error | uint32 LE` (`error` — код вышестоящего протокола; `detail` — opaque, `Nack::kDetailNone` = 0). Ack DLC=0.
- Класс **C**: логика внутри Session (`tick` / link HB).
- Прикладные фасады (`ServerSession::select` …) — над `request`/`send`, не транспорт.


Виртуальный `IMessage` — не правило. Новый PDU = struct + `SMCP_IF_MSG` в нужном листе.

### 4. Узел (Node)

- RX: `Node::update` → `acceptRx` → `sessionByPeer` / `Session::onPacket` → иначе `Node::onPacket`.
- Свой IdConflict: `enterIdConflict` (Fault PDU + `closeAllSessions` + `onFault(nullptr)`).
  Чужой Fault **IdConflict**: лог, сессию не трогаем (cleanup через `HbLost`).
  Чужой Fault **≠ IdConflict**: `close(PeerDown)` у сессии с этим peer.
- Новые **A**: наследник Session → `request(T)` + `onPacket` → `SMCP_IF_MSG(…)` + Ack/Nack.
- Новые **D**: `Node::send(T{}, Packet::kBroadcastId)` и разбор в **наследнике Node** (`onPacket`). Не через Session.
  Fault — **D** (broadcast); локальные `IdConflict` / `HbLost` / `PeerDown` — `Session::onFault` при своём `close`.

### 5. Чеклист «добавил MsgId»

- [ ] Класс A–E выбран; прикладной MsgId записан в каталог `SYSTEM.md` (роль + кто → кому)
- [ ] `kId` / pack/unpack; TX: `request` (**A**) или `send` / TaggedOps
- [ ] TX путь: ReplyWnd (**A**), reply (**B**), internal (**C**), broadcast Node (**D**), notify (**E**)
- [ ] RX путь: наследник Session (unicast) или наследник Node (broadcast)
- [ ] Транспортный MsgId — обновлён этот файл

---

## Два «чипа»

Узлы независимы (`poll` каждый свой `Link`).  
На одном MCU отладка: два `MockCan` + `connect()`.
