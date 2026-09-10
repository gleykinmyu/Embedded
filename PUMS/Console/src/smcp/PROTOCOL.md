# SMCP MVP — протокол (консоль ↔ сервер сегмента)

Канал: CAN extended ID, `smcp::msg` (см. `message.hpp`).  
Wire set: **Ack, Nack, Heartbeat, Select, SetTarget, Telemetry**.

Лимит Select на сегменте — leaf в `acceptSelect`: слияние (чужие Selected ∪ маска консоли) → **SelectLimit**.

---

## Роли сообщений

| Msg | Кто → кому | Смысл |
|-----|------------|--------|
| **Select** | Console → Server | Выделение осей (Select / Deselect / Set) |
| **Ack / Nack** | Server → Console | Принятие / отказ **запроса** (`pkt_id` = запроса) |
| **Telemetry** | Server → (обычно broadcast) | Снимок оси после **изменения** состояния или движения |
| **Heartbeat** | Console → Server (первый ping); далее ping/pong | Линк + «мягкая» регистрация консоли |
| **SetTarget** | Console → Server | Цель движения (позже в базовом poll) |

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

Telemetry при **движении** — отдельно: по Δposition / rate-limit на стороне сервера (наследник / приводной слой). Базовый `IServer` шлёт Telemetry при смене select-состояния.

**Не** трактовать Telemetry как RPC-ответ на Select: ответ на Select — Ack/Nack.

---

## Heartbeat (линк + регистрация)

Open/Close на wire нет. Локально: `Session::Status` / `linkUp()`.

В Node нет ролей «консоль/сервер». Сессия: `Idle` / `Connecting` / `Awaiting` / `Open`
(`Awaiting` = первый ping, ещё не up; keep-alive из `Open` — только `_hb.isWaiting()`).
`_master` (`start(peer)`) — кто шлёт keep-alive HB; без `start` — только pong + сторож тишины.
Кто master, решает приложение (консоль: `startSession(server_id)`; сервер: слоты без `start`).

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

API консоли: `startSession(peer)` / `stopSession`, `serverId`, `update`, `linkUp`.
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

`IConsole` / `IServer` наследуют **Node**, держат `ObjStorage<Session*>`;
объекты `Session` владеет leaf (`MConsole` — одна `Session`, `MServer` — `SessionBank<SessionConsole>`).

### Конфликт одинаковых `console_id`

Сервер **не различает** двух консолей с одним ID — для него это один `src_id`.
Отдельный Register / участие сервера в разруливании дубля **не нужны**.

Правило на консоли (first-wins), всё на **Node**:
1. Перед первым своим Heartbeat — **слушать** шину ~`kHeartbeatTimeoutMs` (`update`/`acceptRx`).
2. Увидела любой кадр с `src_id == мой id` → `Node::IdConflict` → **не RX-demux и не TX**.
3. Уже в сессии: то же — TX/RX стоп; close сессий — в `onStatus` приложения.

Тот же пункт у сервера — для **своего** id.

---

## Очереди

- Очередь **кадров** — в `ICAN` (драйвер / `MockCan`).
- Очередь **bus / broadcast** `(body, dst, pkt_id)` — `Node::_tx` (`Node::send`, класс **D**).
  `TxQueue::isFull` после stall → `onTxFull(nullptr)`.
- Очередь **session unicast** — снаружи одна (`transmit` / `isTxFull`);
  внутри `Session::_tx_req` (A) + `_tx_ctrl` (HB/Ack/Nack).
  Full любой → `isTxFull()`; edge → `onTxFull(session)`.
- Drain: один RR на `sessionCapacity + 1` (слоты сессий + bus), **по одному кадру** за заход.
- Окно Ack = **1**: class A после wire — голова `_tx_req` + `_ack` (не drop до reply);
  `_tx_ctrl` / `_tx_req` — RR в `peekTx` (после успешного wire).
  Retry головы req по timeout; Ack/Nack / `Timeout` → `onAck` → drop + `onReply`.
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

Примеры: Select/SetTarget → **A**; Ack/Nack → **B**; Heartbeat → **C**; Telemetry → **D**.

**pkt_id** — признак сессии (`Packet.pkt_id`), не поле body. На wire для A/B:
`data[0]=pkt_id`, дальше payload body. `msg::helpers::carriesPktId(msg_id)`; body без `pkt_id`.

### 2. Wire / codec (`message.hpp` + `msg::helpers`)

1. Добавить `MsgId` и `struct` body (+ `kId`) — только семантика сообщения.
2. Класс **A**/**B** → `helpers::carriesPktId=true`; codec пишет/читает `Packet.pkt_id` в data[0].
3. Класс **C**/**D**/**E** → `carriesPktId=false`.
4. body `serialize`/`deserialize` = payload **без** pkt_id; `helpers::toCanFrame`/`fromCanFrame` клеят pkt_id.
5. `helpers::requiresAck(id) == true` **только** для класса **A**.
6. `helpers::defaultPrio(id)` — по смыслу (команды выше, telemetry/HB ниже).
7. `msg::Message` (`variant`) — **только codec + RX demux**, не API сессии.

### 3. Session API (не плодить метод на каждый MsgId)

- Класс **A**: `send(Select)` / `send(SetTarget)` или общий  
  `Request = variant<Select, SetTarget, …>` → `send(Request)` (pkt_id внутри Session).  
  Новый request = тип в `Request`, не поле в message и не новый «pkt_id в struct».
- Класс **B**: `sendAck(req_pkt_id)` / `sendNack(req_pkt_id, code)`.
- Класс **C**: логика внутри Session (`tick` / `onHeartbeat`).
- Фасады `IConsole::select` / `setTarget` — над `send`, не транспорт.

Виртуальный `IMessage` — не правило; при росте очереди возможен полиморфизм команд (Nextion),
пока — перегрузки / `Request` variant.

### 4. Узел (IConsole / IServer)

- RX: `Node::update` → `acceptRx` → `sessionByPeer` / `Session::onPacket` → иначе `Node::onPacket`.
- Новые **A** на сервере: `SessionConsole::onPacket` → `onSelect` + Ack/Nack; смена оси — Telemetry (**D**).
- Новые **D**: `Node::send(body, kBroadcastId)`; не через Session.
- Политика отказа Select (`acceptSelect` → SelectLimit/…) / SetTarget (Limits) — в наследниках.

### 5. Чеклист «добавил MsgId»

- [ ] Класс A–E выбран и записан в таблицу «Роли сообщений»
- [ ] Codec + `requiresAck` / `carriesPktId` согласованы с классом
- [ ] TX путь: Session queue (**A**), reply (**B**), internal (**C**), broadcast (**D**), notify (**E**)
- [ ] RX путь на нужном узле
- [ ] `PROTOCOL.md` обновлён (роль + кто → кому)

---

## Два «чипа»

Консоль и сервер — независимые узлы (`poll` каждый свой `Link`).  
На одном MCU отладка: два `MockCan` + `connect()`.
