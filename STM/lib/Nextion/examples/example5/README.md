# Example 5 — все листья nexComponents.hpp

Четыре страницы HMI с **24** виджетами — по одному на каждый листовой класс из `nexComponents.hpp` (без `nexExComponents.hpp`).

После `restartScreen` MCU выставляет `bkcmd=Always` (`enableBkcmdAlways()`) **только на время static demo** — ACK на каждую команду, ошибки атрибутов в `onError` / UART. После `runAllDemos` — `bkcmd=OnFailure` для live loop.

Затем один раз `runAttributeDemoOnce()` → `runAllDemos()`: настройка цветов, геометрии, шрифтов, `enable`.

В main loop — `tickLiveDemos()` каждые ~400 ms: потоковые виджеты, у которых поведение раскрывается со временем:

| Виджет | Статическая настройка | Живое обновление |
|--------|----------------------|------------------|
| **Waveform** | каналы, сетка, `dis` (масштаб) | `ch[].add()` — осциллограмма «едет» |
| **ProgressBar** | фон, bar, скругление | `value` 0↔100 |
| **Gauge** | center, pointer | `setAngle()` 0…360° |
| **Slider** | cursor, bg2 | `value` следует за фазой |
| **Timer** | `setPeriod`, `enable` | тикает на панели |
| **ScrollText** | dir/dis/tim, `enable` | прокрутка на панели |
| **NumericVar / Number / XFloat** | формат, шрифт | `val` зеркалит фазу |
| **SlidingText** | `txt`, `setShowProgressBar` (`left`) | `val_y` |

## Страницы

| id | имя | виджетов | назначение |
|----|-----|----------|------------|
| 0 | `ex5a` | 8 | Timer, переменные, hotspot, QR, картинки, waveform |
| 1 | `ex5b` | 6 | Progress bar, slider, gauge, ComboBox, TextSelect |
| 2 | `ex5c` | 5 | Text, SlidingText, ScrollText, Number, XFloat |
| 3 | `ex5d` | 5 | Button, DualStateButton, Checkbox, Radio, ToggleSwitch |

## Objname на панели

### ex5a (id=0)

| objname | Класс C++ |
|---------|-----------|
| `timer0` | `Timer` |
| `nvar0` | `NumericVar` |
| `svar0` | `StringVar` |
| `hot0` | `Hotspot` |
| `qr0` | `QRCode` |
| `pic0` | `Picture` |
| `crop0` | `CropPicture` |
| `wave0` | `Waveform` |

### ex5b (id=1)

| objname | Класс C++ |
|---------|-----------|
| `pbar0` | `ProgressBar<Color>` |
| `pbari0` | `ProgressBar<Image>` |
| `slid0` | `Slider` |
| `gauge0` | `Gauge` |
| `combo0` | `ComboBox` |
| `tsel0` | `TextSelect` |

### ex5c (id=2)

| objname | Класс C++ |
|---------|-----------|
| `text0` | `Text` |
| `slt0` | `SlidingText` |
| `stext0` | `ScrollText` |
| `num0` | `Number` |
| `xf0` | `XFloat` |

### ex5d (id=3)

| objname | Класс C++ |
|---------|-----------|
| `btn0` | `Button` |
| `dual0` | `DualStateButton` |
| `chk0` | `Checkbox` |
| `rad0` | `Radio` |
| `tsw0` | `ToggleSwitch` |

Разместите виджеты без сильного перекрытия. **Touch:** у интерактивных — *Send Component ID*; id на панели 1…N в пределах страницы.

## Сборка и запуск

```bash
pio run -e example5
```

После `rest` прошивка ждёт 3 с и ставит команды в очередь (см. отладочный UART при `-DNEX_DEBUG`).

## Код

- `app.hpp` — 4 страницы и `AllComponentsDemoApp`
- `demo_controls.hpp` — `runPageADemos` … `runPageDDemos`

## Не в примере

Классы из `nexExComponents.hpp` (Audio, Video, DataRecord, FileBrowser, …) и типы без C++-листьев: TouchCap (5), PageComponent (121).
