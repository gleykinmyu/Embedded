# new_smcp

Описания бинарных протоколов. Реализация обмена, сессий и Jobs сюда не входит.

Сериализация — `libs/bitsery/custom`, пространство имён `bitsery::custom`: MSB-first адаптеры, `Writer`, `Reader`, `Measure`, `putBits`.

```text
proto/
    base/header.hpp       17 бит: prio, dst, src, proto
    control/header.hpp    4 бита: op
    control/pdu.hpp       HeartBeat, Fault, Arp
    msg/header.hpp        7 бит: txn, type
    msg/pdu.hpp           CtrlHeader, Ack, Nack, seg::Header, Begin, End
```

Каждый заголовок и PDU пишет только свои поля. Курсор один на всю цепочку, между объектами выравнивания нет.

Бюджет кадра — 93 бита (`CanLimits`). Каждый уровень вычитает только свой заголовок. `SegData` занимает остаток после `SegHeader`: 6 байт.
