# new_smcp

Пересборка SMCP на `libs/comm` (Node / Session / IProto). Старый `libs/smcp` не трогаем.

## Слой

| файл | роль |
|---|---|
| `addr.hpp` | `addr_t` для `comm::Node` |
| `transport.hpp` | `MsgId` + `Message` + `Frame` |
| `pdu.hpp` | pack/unpack служебных PDU |
| `session_bank.hpp` | `SessionBank<N>`: сессии + `LinkHbRoute` |
| `ibuf.hpp` | `IBuf` для тел Get/Set |
| `proto/` | протоколы (`IProto`) |

## proto/

| | |
|---|---|
| `ncp` | Listen / AddrConflict (`comm::Ncp`) |
| `fault` | broadcast Fault → `Node::onFault` |
| `link_hb` | Heartbeat: Request/Ack на рукопожатии, дальше пустой кадр |
| `link_hb_route` | первый Request без сессии → слот или Heartbeat Busy (член `SessionBank`, в списке узла после банка) |
| `reply` | short RPC Ack\|Nack (`JobListNet`, окно 1) |
| `tagged` | Get / Set + `ITaggedSink` (длинное тело → MultiByte) |
| `multibyte` | DataBegin / Data |
| `segment` | northbound demux + `ISegmentSink` (без Console/Server) |

Уведомления вверх: protected-метод владельца (`Node::onFault`, `Session::onLink` / `onAck`, …) либо sink у протокола (`ITaggedSink`, `ISegmentSink`). Исходящий short RPC сегмента — через `Reply`.

## Дальше

1. CanLink / host ILink под `Frame`
2. Session leaf: состав протоколов + общий `TxnAlloc`
3. Segment PDU pack/unpack и разделение ролей при необходимости
4. Длинный Get/Set (DataBegin→Data*) — host-тесты
