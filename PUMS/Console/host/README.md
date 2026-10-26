# SMCP host — транспортный стенд

Один процесс = один `Node`. Нет прикладного API (Select / Telemetry / …).  
Цель: прогнать механизмы `libs/smcp` и реакции на ошибки на шине.

```bash
make -C host
./host/run_mesh.sh        # bus + 2 node
```

| Файл                | Назначение                                 |
| ------------------- | ------------------------------------------ |
| `build/smcp_bus`    | TCP-хаб + live Nodes/Links                 |
| `build/smcp_node`   | REPL узла                                  |
| `SMCP Bus.command`  | macOS: двойной клик → bus                  |
| `SMCP Node.command` | macOS: двойной клик → node                 |

---

## Механизмы

| Что | Как |
| --- | --- |
| Listen → Ready | `id 0x01` |
| Смена / уход с шины | `id 0x02` / `end` |
| Master / slave HB | `open 0x10` на одной стороне; вторая только `id` |
| Close | `close 0x10` |
| Set/Get inline (≤7) | `set 0x10 0xA1 deadbeef` / `get 0x10 0xA1` |
| Set/Get multi (>7) | `set 0x10 0xA1` + ≥8 байт hex |
| Сырой кадр | `send <dst> <msgid> [txn N] [hex\|\"ascii\"]` |
| Ack / Nack | `ack <dst> <txn>` / `nack <dst> <txn> Busy` |
| Fault (без суицида) | `fault` / `name portal` + чужой дубль id |

## Ошибки на шине

| Реакция | Как спровоцировать |
| ------- | ------------------ |
| **IdConflict** (новичок) | второй узел: `id` уже занятый |
| инкумбент жив, сессии целы | чужой `Fault IdConflict` не рвёт линк |
| **HbLost** | на одной стороне `mute tx` (или `mute rx`) |
| **Nack NotSupported** | на peer: `policy reject`, затем `get`/`set` |
| **Nack Busy** (policy) | на peer: `policy busy`, затем `get` |
| **Nack Busy** (FSM) | свой GetAsk/SetOut + чужой Get/Set; встречные multi Set |
| **MB RX Timeout** | после `get`: peer `mute tx` → `DataBegin::kTimeoutMs` |
| **MB Overflow** | `get` в буфер меньше тела |
| **Abort** | `close` mid SetIn/SetOut |
| **Nack Body** | сырой `send` Data с битым seq в чужой txn |
| **Ack timeout** | peer: `mute tx` во время чужого `set` |
| **SessionFull** | 16 живых сессий, ещё `open` |
| повтор после конфликта | `clear` + `id <hex>` |

Один MultiByte на сессию (Tx **или** Rx); TaggedOps — один job (`isBusy`). Одновременный full-duplex MB нет.

`mute` глушит только этот сокет: TX «успешен» локально, в шину не уходит; RX сливается.  
`policy` — как **этот** узел отвечает на чужие Get/Set (`echo` / `reject` / `busy`).

---

## Команды

| Команда | |
| ------- | - |
| `id <hex>` | `begin`, Listen → Ready |
| `name [str]` | короткое имя в Fault |
| `end` | `Node::end` → Idle |
| `open` / `close <peer>` | сессия |
| `set` / `get` | tag `0xA1` echo |
| `send` / `ack` / `nack` / `fault` | сырой транспорт |
| `mute` / `policy` | инъекция тишины / Nack |
| `clear` / `status` / `errors` / `wait` / `history` / `help` / `quit` | |

```text
node> id 0x01
n01> open 0x10
n01> set 0x10 0xA1 0123456789ABCDEF
n01> get 0x10 0xA1
```

На другом окне достаточно `id 0x10` (slave откроется по HB).

```bash
make -C host check      # in-process матрица ошибок
./host/smoke_mesh.sh    # TCP: bus + 2 node
```
