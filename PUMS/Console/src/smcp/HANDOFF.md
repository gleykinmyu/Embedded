# SMCP — выжимка для следующего агента

Архитектура. Вынос north-PDU из транспорта **сделан**. Второй `Node` (CAN2) в коде **не начинали**.

## Контекст

SMCP сейчас — **northbound**: консоль ↔ сервер сегмента по **CAN1**.
Механизмы с сервером по SMCP **не общаются**.
`IMech` / `DriveMech` — локальный inventory сервера (`mech_id` 0…31), не узлы шины.
`DriveMech::setTarget` — заглушка.

Будет **вторая шина CAN** (CAN2) под железо механизмов. Сервер — шлюз.
Свои платы — второй `Node` + `SessionDrive`. Чужие частотники CiA 402 — отдельный мастер на сервере, не SMCP.

## Решения

- Плата механизма строится на том же классе **`Node` + `Session` + `CanLink`**.
- На сервере **два `Node`**: CAN1 (`IServer`, сессии консолей) и CAN2 (сессии плат).
  Не второй линк в существующем `IServer`.
- `DriveMech` мапит устройство(я) на CAN2 → логический `IMech` для пульта.
  Композиция плат — только на сервере. Пульт видит «ось N на `server_id`».
- Southbound **не** слой над `Select`/`Telemetry` и не вложенный протокол в payload.
- Транспорт: HB, Ack/Nack, `pkt_id` в CAN ID, очереди, `IdConflict`.
  North PDU: `Console/console_message.hpp`. Drive PDU — свой файл, не `message.hpp`.
- Конверт: `Packet { src, dst, pkt_id, Message { id, data[8], dlc } }`. Мирового `variant` нет.
  Demux: `SMCP_IF_MSG` в `SessionConsole` (unicast A) / `IConsole::onPacket` (class D).
- Независимость протоколов: непрозрачный payload + диапазоны `msg_id`.
- **`pkt_id`** в ID (6 бит, LSB). Между узлами арбитраж до него не доходит (уникальный `src`).
  Class C/D/E: `pkt_id=0`. Не поле Select/Telemetry.
- Не класть opcode внутрь data. Не резать `msg_id` на proto+cmd в ID.

## ID на шине (north)

- Консоли `0x01…0x0F`, серверы `0x10…0xEF`, broadcast `0xFF`. `0` не занимать.
- CAN ID: `msg_id[7] | dst[8] | src[8] | pkt_id[6]` в битах `[28:0]`.
- Приоритет = `msg_id`: Ack/Nack → команды → HB `0x30` → Telemetry `0x40`.
- Payload class A без `pkt_id` в data (Select DLC=5, SetTarget DLC=7, Ack DLC=0, Nack DLC=2 `error|detail`).
  Коды Nack — у northbound (`ErrorCode` в `console_message.hpp`), транспорт несёт raw `uint8_t`.

## Что не делать

- Тащить `Select`/`Block`/`holder` на CAN2.
- Расширять `message.hpp` приводными / north PDU.
- Второй стек «поверх SMCP» с заголовком в data.
- Один `Node` на два `ILink`.
- Разбор broadcast в Session — только в наследнике Node.

## Код

- Протокол: `PROTOCOL.md` (классы A–E, чеклист MsgId).
- Транспорт: `transport/{node,session,message,ilink,can_link}`.
- North PDU: `Console/console_message.hpp`.
- Сервер: `Server/`, `src/Server/model/{mserver,drive_mech}`.
- Плата: только CAN1 (`board.can`, PD0/PD1). F407 умеет CAN2 — ещё не открыт.
