# SMCP — разбор кода

Segment Motion Control Protocol: `src/smcp`, leaf `MConsole` / `MServer` / `DriveMech`, UI пульта и сегмента.

Источник — дерево Console, сентябрь 2026. Два прохода: протокол/транспорт, затем шина + стеки UI.

Объём `src/smcp` ≈ 6.3k строк (transport ~2.4k, Console/шоу ~2.3k, Group ~0.7k, Server ~0.5k). На шине 8 `MsgId`, классы PDU A–E.

## Вердикт

Транспорт и wire-контракт собраны аккуратно: классы A–E, окно Ack = 1, plan→accept→commit, Listen на IdConflict.

Слабое место — жизненный цикл владения осью и то, что политики UI живут на кнопках, а не в модели. Для одного сегмента и mock-loopback этого хватает; для нескольких пультов на живой шине — нет.

Модель угроз — не интернет-сервис. Атакующий: узел на CAN, поддельный шоуфайл на SD, UART к Nextion, ошибка оператора. Криптографии на шине нет и не планировалось.

## Слои

Зависимости сверху вниз. Утечка одна: `message.hpp` тянет `Selection` из GroupConsole.

| Слой | Типы | Ответственность |
|------|------|-----------------|
| Шоу / UI-модель | Show, CGroupBank, FIOManager, Browser, MConsole | GRUP/SETT на диске, recall, каталог файлов |
| Узел приложения | IConsole, IGroupConsole, IServer, SessionConsole | Phase, Select/Block/SetTarget, политика, Telemetry |
| Домен оси | IMech, CMech, CGMech, DriveMech | holder / status / position; TX с пульта, commit на сегменте |
| Сессия / узел | Node, Session, TxQueue | HB, pkt_id, очереди req/ctrl/bus, IdConflict, RR drain |
| Wire | msg::*, ILink, CanLink, MockCan | 29-bit CAN ID, DLC, codec Packet ↔ Frame |

## Поток кадра

1. `ILink::receive` — Frame → Packet, DecodeFailed рвёт кадр
2. `Node::acceptRx` — src==наш → IdConflict; иначе dst нам / FF
3. `sessionByPeer` / `openNewSession` — unicast HB открывает свободный слот
4. `Session::onPacket` — HB / Ack / Nack съедаются здесь
5. `SessionConsole` / `IConsole` — Select, Block, GetTelemetry, SetTarget, Telemetry
6. `pumpTx` RR — слоты сессий + bus, один кадр на слот за update

## Wire set

| Msg | Класс | Кто → кому | Очередь | Замечание |
|-----|-------|------------|---------|-----------|
| Select / Block / GetTelemetry / SetTarget | A | Console → Server | Session::_tx_req | ждёт Ack/Nack, retry 3×100 мс |
| Ack / Nack | B | Server → Console | _tx_ctrl | Nack: code + detail (обычно mech_id) |
| Heartbeat | C | master ping / slave pong | _tx_ctrl | DLC=0, без pkt_id |
| Telemetry | D | Server broadcast | Node::_tx | не RPC-ответ на Select |

## Lifecycle узлов

### IConsole::Phase (id 0x01–0x0F)

`begin` → Listen (~500 мс) → Connecting (HB master) → Online. Потеря линка → снова Connecting. IdConflict / RegisterFailed → Fault, primary close.

`MConsole::readyForOnline` ждёт `setUiReady`. Online → GetTelemetry по всем слотам inventory. Nack Select/Block/SetTarget с detail → точечный GetTelemetry.

### IServer политика (id 0x10–0xEF)

`SessionConsole`: план маски → `mechGuard` → `accept*` → commit → Ack → Telemetry по changed. Drop Select раньше take — чтобы не упереться в лимит.

`MServer::acceptSelect`: зона 7–9 не вместе + merged holders ≤ 3. `acceptBlock` / `acceptSetTarget` по умолчанию Ok. `DriveMech::setTarget` — пустой TODO.

### Линк

- Master (пульт): Connecting → ping → Awaiting → Open
- Slave (сервер): bind по unicast HB, только pong + miss
- T = 500 мс, miss max = 3 → close
- `isOpen` = только Open. Awaiting ≠ online. IdConflict режет TX/RX, сессии сам Node не рвёт.

## Стеки UI

**Console (10″, 6 страниц).** `UiConsole` : `MConsole` → хуки в `Application`. Страницы: wait, work, mGroup, mFile, browser, settings. Nextion UART 250000, без pairing. Политики Mode/Block/isolate проверяются в `onTouch` отдельных кнопок, не на входе страницы и не на сервере.

**Server (4.3″, page0).** Один SlidingLog (`cnsl`) + StatusBar. serial1 лог зеркалится на панель. Сборка `env:server`: `SMCP_DEBUG` + `TRACE_LINK` — кадры шины на HMI. Управления осями с панели сегмента нет. Риск — утечка трассы и забитый UART, не ложный Select.

## Находки

| Sev | Тема | Где | Суть |
|-----|------|-----|------|
| Crit | Select живёт дольше сессии | IServer / DriveMech | `hbLost` закрывает Session, holder не сбрасывается. Чужой пульт получает Busy, пока тот же `console_id` не вернётся или кто-то не Block. |
| Crit | Telemetry без фильтра сегмента | IConsole::onTelemetry | Зеркало оси принимает любой src с `isServerId`. Два сегмента с `mech_id=0` перезапишут друг друга. |
| Crit | Привод не подключён | DriveMech::setTarget | Ack + Telemetry после accept, но позиция/Moving не меняются. `resetFault` пустой с обеих сторон, MsgId нет. |
| Crit | Один кадр = IdConflict DoS | Node::acceptRx | `src == наш id` → стоп TX/RX. Пульт уходит в Fault. Сервер не вызывает `clearError` — мёртв до ресета. Подделать src на CAN тривиально. |
| Crit | Спуф Ack / HB / Telemetry | Session::onAck, onHeartbeat | Нет подписи. Кадр с src сервера закрывает окно Ack или держит HB, пока реальный сегмент молчит. Зеркало и «успех» Select рисует атакующий. |
| Crit | Клетка в Block = сегментный Block | WorkPage::onCellPress | `CMech::block()` шлёт Block на сервер (все пульты, снятие Select). Кнопка группы в том же режиме — только GRUP в шоуфайле. Один `_blockOn`, две семантики. |
| Warn | Inventory 24 ≠ 32 | MConsole / kMechCount | `GroupConsole<24>`, сервер и Selection — 32 оси. GRUP может выделить 24…31: сервер держит, пульт не зеркалит. |
| Warn | queued group без TX | CGroup::recall | `setSelection` + `setQueuedGroup` без проверки isOpen/enqueue. Поздний чужой Select Ack активирует группу. |
| Warn | Тихие отказы | Session::send, handleMaskOp | !Open / TxFull / !isConsoleId — no-op или return без Nack. UI не отличает «ушло» от «съедено». |
| Warn | Двойной лимит Select | DriveMech::s_selectedCount | static на процесс + `acceptSelect`. Расхождение → Ack при no-op select, Telemetry с holder=0. |
| Warn | Любой 0x01…0x0F — консоль | SessionConsole | HB bind + Select/Block/SetTarget. `acceptBlock` = Ok всем. Action::Set с пустой маской снимает Block со всего сегмента. |
| Warn | replaceWith теряет dest | IBrowser::replaceWith | Существующий файл удаляется до `rename(tmp)`. Сбой rename → шоу нет ни на месте, ни в tmp. |
| Warn | isolateGroup только в UI | WinchButton::isolated | Глушит Press клетки. recall группы всё равно выделяет «изолированные» оси. На сервере флага нет. Block-режим isolate не смотрит. |
| Warn | Show не запирает Select | onMenuPress / onCellPress | Show/Block отключают File и Block. Клетки и группы по-прежнему шлют Select. Режим — косметика меню. |
| Warn | Политика на кнопке, не на странице | mFile / browser / settings | `assignOk()` только у bFile. HMI page / 0x65 на UART открывают файл и RTC в Show. MCU `onLoad` не проверяет mode. |
| Warn | Хуки SMCP → MsgBox / Nextion | UiConsole::onNack, onMechChanged | `showSmcpNack` и sync клетки внутри `Node::update`. UART панели тормозит pump CAN; вложенный enqueue может уйти в stall. |
| Info | Block без владельца | acceptBlock | Любая консоль блокирует любую ось и снимает Select. Задокументировано, опасно на общей шине. |
| Info | Слой codec↔GRUP | message.hpp | Selection живёт в `group.hpp`. Wire зависит от шоу-модели. |
| Info | Недобитое | Group::Atomic, ErrorCode::Crc, accel | Флаг Atomic не используется. CRC на кадре нет. `accel_mm_s2` обнуляется на decode. |
| Info | Лишний слот сессии пульта | Console&lt;N,1&gt; = 2 слота | Чужой unicast HB садится на свободный слот, ест RR drain и отвечает pong. Команды A пульт с него не принимает. |
| Info | Select по Press, не Release | onCellPress | Случайное касание и одиночный 0x65 Press с UART. Disabled/NotReady клетка всё равно зовёт `trySelect` (сервер ответит Nack). |
| Info | OemString 96 vs msg 160 | enc::OemString / workPage | Текст «блокировано группами …» режется. Не overflow — потеря причины отказа. |

## Что сделано хорошо

**Контракт.** `PROTOCOL.md` жёстко делит классы A–E, `pkt_id` только в Packet, чеклист нового MsgId. Select — атомарный план, Telemetry не считается RPC-ответом. GRUP Blocked явно отделён от сегментного `Status::Blocked`.

**Транспорт.** Две очереди сессии (req с окном 1, ctrl без Ack), bus для broadcast, stall enqueue с лимитом глубины, RR drain. Listen + src==наш id ловит дубль консоли без участия сервера.

## Что проверено и не дыра

| Тема | Почему не поднимаю |
|------|-------------------|
| Инъекция Nextion через имя группы / файла | `libs/Nextion` `printQuotedString` экранирует `"`, `\\` и `0xFF` → `?`. |
| Path traversal в браузере | `isValidName` режет `/ \ :` и `..`. `makePath` только basename + cwd. |
| CRC шоуфайла | header CRC16 + body CRC32, staging → live. Злой файл с верным CRC — валидные маски, не overflow. |
| W25Q bak переполнение | write клипится сектором; `static_assert` размер шоу ≤ 4K. |

## Порядок, если чинить

1. `IServer::onHbLost`: снять Select этого peer, Telemetry по сменившимся осям. Иначе мёртвый holder.
2. `IConsole::onTelemetry`: принимать только `hdr.src_id == serverId()`. Иначе сегменты смешают зеркало.
3. Развести UI: клетка в Block → только GRUP или отдельная кнопка «сегмент». Не слать Block маской с клетки по умолчанию.
4. IdConflict: серверу — `onStatus` → close всех сессий + явный recover; на шине это всё равно DoS, но не «zombie Open».
5. Выровнять ёмкость пульта с `kMechCount` или резать маску GRUP по `mechCapacity()`. Убрать `static s_selectedCount` в инстанс сервера.
6. `CGroup::recall`: queued только после успешного `Session::send`. Любой Select вне recall — clear queued/active.
7. `isolateGroup` и `Mode::Show` проводить в модели (`trySelect` / `recall` / `IConsole::select`), не только в цвете кнопки. `onLoad` страниц — тот же guard.
8. Убрать SMCP/NEX trace из `env:server` / `env:console`. `onNack`/`onTelemetry` — не открывать MsgBox синхронно из `Node::update`.
9. `replaceWith`: rename на dest.bak, потом dest; не unlink dest до успеха.
10. Когда появится привод — `acceptSetTarget` (Limits), Moving/position в DriveMech, MsgId ResetFault. До этого не считать MVP «движением».
