# SMCP — выжимка для следующего агента

Архитектура, не реализация. Вынос PDU / второй `Node` в коде **не начинали**.

## Контекст

SMCP сейчас — **northbound**: консоль ↔ сервер сегмента по **CAN1**.
Механизмы с сервером по SMCP **не общаются**.
`IMech` / `DriveMech` — локальный inventory сервера (`mech_id` 0…31), не узлы шины.
`DriveMech::setTarget` — заглушка.

Будет **вторая шина CAN** (CAN2) под железо механизмов. Сервер — шлюз.

## Решения

- Плата механизма строится на том же классе **`Node` + `Session` + `CanLink`**.
- На сервере **два `Node`**: CAN1 (`IServer`, сессии консолей) и CAN2 (сессии плат).
  Не второй линк в существующем `IServer`.
- `DriveMech` мапит устройство(я) на CAN2 → логический `IMech` для пульта.
  Композиция плат — только на сервере. Пульт видит «ось N на `server_id`».
- Southbound **не** слой над `Select`/`Telemetry` и не вложенный протокол в payload.
- Транспорт заморозить: HB, Ack/Nack, `pkt_id`, очереди, `IdConflict`.
  Прикладные PDU — отдельные файлы, свои `MsgId`, наследник сессии
  (`SessionConsole` / будущий `SessionDrive`).
- `Packet` сейчас `{Header, pkt_id, variant body}`.
  Цель: конверт `{hdr, pkt_id, data[8], dlc}` без мирового `variant`.
  Demux приложения — `switch (msg_id)` → свои struct.
  Локальный variant только у протокола — по желанию.
- Кадр CAN **не перенарезать** ради независимости протоколов.
  Независимость: непрозрачный payload + диапазоны `msg_id`.
- **`pkt_id` нужен** (корреляция class A ↔ Ack/Nack при retry/timeout).
  У HB/Telemetry — нет.
- Не класть opcode внутрь data «чтобы протоколы не пересекались».
  Не сжимать `msg_id` до 5 бит как приоритет, если потом всё равно нужен внутренний тип.
- Если когда-нибудь выносить `pkt_id` в ID ради 8 байт payload:
  лучше коротко `prio` + 8-битный `msg_id` + короткий `pkt_id`,
  не `msg5|dst|src|pkt8`.
  Сейчас 7 байт у class A терпимо — раскладку можно не трогать.

## ID на шине (north)

- Консоли `0x01…0x0F`, серверы `0x10…0xEF`, broadcast `0xFF`.
- CAN ID сейчас: `prio[28:24] | dst[23:16] | src[15:8] | msg_id[7:0]`.
- Class A: `data[0]=pkt_id`, дальше body. C/D: только payload.

## Что не делать

- Тащить `Select`/`Block`/`holder` на CAN2.
- Расширять `message.hpp` новыми приводными PDU.
- Второй стек «поверх TCP-SMCP» с заголовком в data.
- Один `Node` на два `ILink`.

## Код

- Протокол: `PROTOCOL.md` (классы A–E, чеклист MsgId).
- Транспорт: `transport/{node,session,message,ilink,can_link}`.
- Сервер: `Server/`, `src/Server/model/{mserver,drive_mech}`.
- Плата: только CAN1 (`board.can`, PD0/PD1). F407 умеет CAN2 — ещё не открыт.
