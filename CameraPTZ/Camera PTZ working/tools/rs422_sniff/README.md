# RS422 / RS485 multi-channel raw sniffer

Автономный sniffer: слушает один или несколько COM-портов USB↔Serial
(например Waveshare 4-ch FT4232), помечает **чей TX** слышен на каждом порту,
пишет **сырые чанки** в лог. Без разбора протокола (не привязан к ccam / Convertible).

Папку можно копировать в другой репозиторий целиком.

## Contents

| File | Role |
|------|------|
| `sniff.py` | Sniffer (Python 3.10+) |
| `config.json` | Порты, baud, направление TX |
| `requirements.txt` | `pyserial` |
| `logs/` | Логи сессий |
| `README.md` | Документация |

## Requirements

```bash
pip install -r tools/rs422_sniff/requirements.txt
```

## Config (`config.json`)

| Field | Meaning | Default |
|-------|---------|---------|
| `baud` | Скорость | `9600` |
| `bytesize` / `parity` / `stopbits` | Формат линии | `8` / `N` / `1` |
| `flush_timeout_ms` | Пауза на линии → граница чанка в логе | `80` |
| `show_hex` | Печатать hex | `true` |
| `show_ascii` | Печатать ASCII-превью (`\\xHH` для непечатных) | `true` |
| `log_dir` | Папка логов | `logs` |
| `channels[]` | Список портов | — |

Поле `framing` **не используется** (если осталось в старом конфиге — игнорируется с предупреждением). Всегда raw.

### Channel

```json
{
  "name": "line_a",
  "port": "COM3",
  "tx_from": "controller",
  "enabled": true,
  "baud": 9600,
  "comment": "optional"
}
```

| `tx_from` | В логе | Смысл |
|-----------|--------|--------|
| `controller` | `CTRL` | Слушаем TX контроллера |
| `device` | `DEV` | Слушаем TX исполнителя |

Алиасы: `ctrl`/`master`/`host` → controller; `dev`/`slave`/`actuator` → device.

## Run

```bash
python tools/rs422_sniff/sniff.py --list-ports
python tools/rs422_sniff/sniff.py --self-test
python tools/rs422_sniff/sniff.py
```

Остановка: `Ctrl+C`. Лог: `logs/rs422_YYYYMMDD_HHMMSS.log`.

## Output format

Всегда **RAW** — байты как есть. Граница записи = пауза ≥ `flush_timeout_ms` (или большой кусок).

```
14:02:11.123  [line_a  ]  CTRL  RAW  7B  |  02 4f 41 57 3a 31 03  |  \x02OAW:1\x03
14:02:11.140  [line_b  ]  DEV   RAW  1B  |  06                    |  \x06
14:02:10.001  [line_a  ]  CTRL  SYS  OPEN COM3@9600 tx_from=controller
```

| Column | Meaning |
|--------|---------|
| `[name]` | Имя канала |
| `CTRL` / `DEV` | `tx_from` |
| `RAW` / `SYS` | Данные или служебное |
| `NB` | Длина чанка |
| hex | Сырые байты |
| ascii | Только подсказка: печатные 0x20–0x7E, иначе `\xHH`. Разбор — по hex |

Разбор протокола — отдельно (агент / человек по hex), не в sniffer’е.

## Wiring

- Sniffer только читает.
- Обычно два канала: TX контроллера + TX исполнителя, у каждого свой `tx_from`.
- Waveshare 4-ch FTDI: `VID:PID=0403:6011` → несколько COM.

## Agent playbook

1. README + `config.json`.
2. `--list-ports` → сопоставить COM.
3. Задать `port`, `name`, **`tx_from`**, `enabled`.
4. `--self-test`.
5. Запуск sniffer → лог из `Log: …`.
6. Действия пользователя ↔ время в логе; колонка `CTRL`/`DEV`.
7. Протокол разбирать по hex/ascii в логе и по документации проекта.
8. `Ctrl+C` перед сменой портов.

```powershell
pip install -r tools/rs422_sniff/requirements.txt
python tools/rs422_sniff/sniff.py --list-ports
python tools/rs422_sniff/sniff.py --self-test
python tools/rs422_sniff/sniff.py
```
