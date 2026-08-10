# SMCP MVP — протокол (консоль ↔ сервер сегмента)

Канал: CAN extended ID, `smcp::msg` (см. `message.hpp`).  
Wire set: **Ack, Nack, Heartbeat, Select, SetTarget, Telemetry**.

Лимит одновременного Select — `acceptSelect` → Nack **SelectLimit**; **Limits** — концевики/`SetTarget`.

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

Ролей в `Session` нет; Open/Close на wire нет. Локально: `isOpen()` / `linkUp()`.

Исходящий keep-alive **не** стартует сам: слой снаружи вызывает `startSession()` /
`Session::start()` (консоль — после `setServerId`). Дальше `tick` шлёт ping раз в T.
Сервер в MVP обычно **не** вызывает `start` — только отвечает; при желании может
`startSession()` и сам пинговать (тот же код).

Сервер может узнать `console_id` с первого RX (`src` → peer).

```text
Слой:     startSession()          (один раз / после смены peer)
Session:  HB → peer               один timer = T (kHeartbeatTimeoutMs), awaiting
Peer:     HB → back               (pong, если сам не ждал ответ)
Session:  open; awaiting=false; timer заново = T
…
timer: нет pong / тишина / пора keep-alive → open=false;
       если start() — сразу новый ping
```

Правила (симметрично, **один** `MsTimer`):
- RX HB и **ждём** → pong: `open`, `awaiting=false`, timer restart, **не** отвечать.
- RX HB и **не** ждём → чужой ping: `open`, timer restart, **один** pong.
- Timeout: down; при `start()` — reconnect/keep-alive ping.
- «Сессия открыта» = успешный обмен HB, не отдельное сообщение.

API: `setServerId`/`setConsoleId`, `startSession`/`stopSession`, `update`, `linkUp`,
`Node::getStatus`/`clearError`, `setClock` (для stall при TxQueueFull).

RX любого кадра с `src_id == мой id` → **`Node::Status::IdConflict`**, TX стоп
(детект в `Node::acceptRx` с первого RX — до открытия сессий).

`Node::send` при полной TX-очереди крутит `update()` до `SMCP_TX_STALL_MS` (как nex enqueue).

### Link / Session / Node

| | |
|--|--|
| **Link** | CAN ↔ encode; один shot `send` |
| **Node** | Link + TX + registry Session*; pump RX/`acceptRx`/session/`onPacket` + TX; **IdConflict** |
| **Session** | peer HB/`isOpen`, pkt_id; TX через `Node::send` (молчает при IdConflict) |

`IConsole` / `IServer` наследуют **Node**, держат `ObjStorage<Session*>`;
объекты `Session` владеет leaf (`MConsole` — одна, `MServer` — `SessionBank` до `kMaxConsoles`).

### Конфликт одинаковых `console_id`

Сервер **не различает** двух консолей с одним ID — для него это один `src_id`.
Отдельный Register / участие сервера в разруливании дубля **не нужны**.

Правило на консоли (first-wins), всё на **Node**:
1. Перед первым своим Heartbeat — **слушать** шину ~`kHeartbeatTimeoutMs` (`update`/`acceptRx`).
2. Увидела любой кадр с `src_id == мой id` → `Node::IdConflict` → **не выходить в TX**.
3. Уже в сессии: то же — Node ставит IdConflict, `Session::stop()`.

Тот же пункт у сервера — для **своего** id.

---

## Очереди

- Очередь **кадров** — в `ICAN` (драйвер / `MockCan`).
- Очередь **исходящих SMCP** `(body, dst, pkt_id)` — `MISC::RingBuffer<Outbound>` в **Node**.
- Очередь надёжной доставки / окно Ack — **в Session** (запросы `requiresAck`).
- Dual-axis Telemetry — **не** в MVP.

---

## Правила расширения (обязательные)

Цель: новый `MsgId` добавляется по чеклисту, без споров «куда метод / куда очередь».

### 1. Класс сообщения (выбрать ровно один)

| Класс | Признаки | Слой TX | `pkt_id` | Очередь Session |
|-------|----------|---------|----------|-----------------|
| **A. Session request** | unicast → peer, ждёт Ack/Nack | `Session` | `Packet.pkt_id`, TX назначает | да (retry/окно) |
| **B. Session reply** | Ack/Nack (или аналог) на request | `Session::sendAck/Nack` | echo → `Packet.pkt_id` | нет |
| **C. Session control** | линк/сессия без RPC (сейчас HB) | внутри `Session` | нет | нет |
| **D. Broadcast / announce** | `dst=0xFF`, без диалога | `Node::send(body, kBroadcastId)` | нет | нет |
| **E. Unicast notify** | unicast, **без** Ack (событие) | `Session` / `Node::send` | нет | нет |

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
- Новые **A**: обработчик на сервере + Ack/Nack; при изменении состояния оси — Telemetry (**D**).
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
