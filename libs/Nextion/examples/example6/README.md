# Example 6 — latency bench (тот же HMI, что example5)

**Одна NIS-команда → ждём matched `Success` (до 2000 ms) → следующая.**

`bkcmd=Always` — ACK на каждую команду. Во время замера `printf` на serial1 отключён (`setSerial1LogEnabled`).

## Протокол замера

1. `drainPanelAcks()` — idle + 50 ms drain RX (сброс осиротевших ACK)
2. `enqueue` одной команды
3. `update()` до **matched** `Success` (`page/comp` = ожидаемый маршрут) или таймаут
4. RTT = tick(matched `Success`) − tick отправки
5. Следующая команда

Исключены из прогона: `waveform.add`, `pbar_color.*` (не attribute-write / ошибки панели).

`switchPage`: tx-маршрут и ACK — **p0 c0** (как в `Application::switchPage`).

## Сборка

```bash
pio run -e example6
pio run -e example6 -t upload -t monitor
```

Summary после бенча: `ok=N/45`. FAIL показывает `(timeout)` или `(rx 0xXX)`.
