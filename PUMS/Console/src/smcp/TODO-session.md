# Session — TODO (продолжить здесь)

Связанный протокол: `PROTOCOL.md`.  
Код: `transport/session.hpp|.cpp`, `transport/node.hpp|.cpp`.  
Консоль: `model/mconsole.cpp` (`setServerId` + `startSession`).  
Сервер: `model/mserver.hpp` (`SessionBank`, `start()` на всех слотах, **peer не задан**).

Статус линка: `Idle` / `Connecting` / `Open`.  
Открытие = `Open` после принятого HB, не `start()`.  
`start()` = «сами пингуем»; `peer_id` = SRC на шине; `id()` = слот registry.

---

## 1. Сейчас сломано: первый HB не попадает в свободную сессию

`Node::update` отдаёт кадр только так:

```text
s = sessionByPeer(src)     // peerId() == src
если s: s->onPacket
иначе:  Node::onPacket     // IServer ест только Select
```

Сессия с `peer_id == 0` **никогда** не получит HB.  
Ветка в `Session::onHeartbeat` (`peer == 0 → peer = src`) **мертвая**.

- **Консоль** работает обходным путём: `setPeerId(сервер)` до ping, pong находит слот.
- **Сервер:** банк слотов с `peer == 0`. HB консоли `sessionByPeer` не находит → кадр пропадает. PROTOCOL «узнать console_id с первого RX» не выполняется.

### Согласованный алгоритм (ещё не в коде)

**Demux в Node** (после `acceptRx`):

```text
s = sessionByPeer(src)

если s == null
     и msg == Heartbeat
     и dst == наш node id          // только unicast, не broadcast
     то s = первая сессия с peer_id == 0

если s != null и s->onPacket(pkt)  // HB съеден
     то конец

иначе Node::onPacket               // Select / Telemetry / …
```

Свободный слот = `peer_id == 0` (Idle или Connecting).  
Нет свободного → HB игнор.  
**Select до HB слот не занимает.**  
**Не звать `setPeerId` из Node** — там `closeLink`. Bind только в `onHeartbeat`.

**Session::onHeartbeat** (кадр уже наш):

```text
IdConflict / src == 0     → выход
peer == 0                 → peer = src      // bind
src != peer               → выход
status = Open
timer = T
awaiting                  → awaiting=false, НЕ pong
иначе                     → один HB в _tx    // pong
```

`closeLink` **peer не сбрасывает** (reconnect к тому же узлу).

Сценарии после фикса:

```text
Консоль: setServerId → start → Connecting → tick ping
         RX HB сервера → sessionByPeer сразу → Open, без pong

Сервер:  peer=0, tick не пингует
         RX unicast HB → свободный слот → bind + Open + pong
         start() на сервере не обязателен
```

### Открытый вопрос (не блокер MVP)

Забывать `peer` после N неудачных reconnect, чтобы слот снова стал `peer == 0`?  
Сейчас 16-я консоль слот не получит, пока живы 15 привязок. Для MVP: не трогать.

### Файлы

- `transport/node.cpp` — `update()`: bind свободного слота на unicast HB.
- `transport/session.cpp` — `onHeartbeat` уже почти совпадает; проверить, что bind не через `setPeerId`.
- `PROTOCOL.md` — дописать demux «свободный слот + первый unicast HB».

---

## 2. Главный пробел Session: окно Ack (класс A)

PROTOCOL: *«окно Ack / pending retry — в Session, позже поверх той же очереди»*.

Заглушки:

- `onTxResult` пустой
- Ack/Nack в `onPacket` **съедаются**, до `IConsole` не доходят
- `kAckTimeoutControlMs` / `kAckRetryControl` не используются
- `send(Select)` fire-and-forget; UI не видит Nack (TODO.md F2)

Нужно:

1. Pending на голове (окно 1): A ушёл на wire → ждать Ack/Nack с тем же `pkt_id`.
2. **Второй** таймер (HB 500 ms и Ack 100 ms нельзя мешать).
3. Retry до `kAckRetryControl`, тот же `pkt_id`; исчерпали — abort + колбэк.
4. Колбэк наверх: Ack/Nack → приложение (`onReply` / аналог на `Node`).
5. Пока pending — не принимать следующий `requiresAck` (не крутить `_pkt_tx`).
6. `closeLink` чистит `_tx` вместе с pending → явный abort + onReply.

Каркас очереди и HB для этого уже есть. Делать **после** п.1 (иначе сервер не привяжет консоль).

---

## 3. Мелочи Session (после 1–2)

| Что | Зачем |
|-----|--------|
| `send()` не требует `Open` | Select может уйти в Connecting |
| `setStatus` без колбэка | Connecting/Open только опросом `getStatus()` |
| `IdConflict` не стопает сессии | tick молчит, `Open` может врать; PROTOCOL: «стоп сессий — позже» |
| Слушать шину ~T до первого ping | старт **консоли** (`MConsole`), не внутренности Session |

Не Session: SetTarget в poll `IServer` (PROTOCOL «позже»); UI Nack/StatusBar — потребители колбэков.

---

## 4. Уже сделано (не переделывать без нужды)

- `Idle` / `Connecting` / `Open`; `start` / `stop` / `closeLink`
- HB ping/pong, `_awaiting`, один `MsTimer` на keep-alive
- Своя `TxQueue`; регистрация `registerAuto` (без slot в ctor)
- `SessionBank`; drain, пока не Idle
- `pkt_id` для `requiresAck`, `++` только если кадр в очереди
- `sendAck` / `sendNack`
- Full: `isTxFull()`; каждая неудачная постановка → `Node::onTxFull(session)`  
  bus: `onTxFull(nullptr)`. Параметра `bool full` нет (только «стали Full»).  
  `TxQueueFull` из `Node::Status` убран (`OK` < `LinkError` < `RegisterFailed` < `IdConflict`)
- `Node::onTxResult`: bus по умолчанию drop на fail; Session на fail голову держит (задел pending)

---

## Порядок на другой машине

1. Demux: свободный слот + первый unicast HB (п.1) — без этого сервер мёртв.
2. Окно Ack + onReply (п.2) — без этого Select не RPC.
3. Политика `send` только из `Open`, IdConflict → stop сессий, listen-before-ping на консоли.
