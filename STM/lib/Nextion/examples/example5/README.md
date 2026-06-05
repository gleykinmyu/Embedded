# Example 5 — все конечные виджеты

Одна страница HMI (`id=0`, имя `ex5`) с **22** компонентами — по одному на каждый листовой класс из `nexComponents.hpp`.

При старте (и при `onLoad` страницы `ex5`) MCU один раз вызывает `AllComponentsDemoApp::runAttributeDemoOnce()` → `ex5::runAllDemos()` (`demo_controls.hpp`): для каждого виджета — все доступные `set*` / `enable*` / `attr::` (зеркало `user`).

## Objname на панели

| objname | Класс C++ | Тип в Nextion Editor |
|---------|-----------|----------------------|
| `timer0` | `Timer` | Timer |
| `nvar0` | `NumericVariable` | Variable, sta=Number |
| `svar0` | `StringVariable` | Variable, sta=String |
| `hot0` | `Hotspot` | Hotspot |
| `qr0` | `QRCode` | QRCode |
| `pic0` | `Picture` | Picture |
| `crop0` | `CropPicture` | Crop Picture |
| `wave0` | `Waveform` | Waveform |
| `pbar0` | `ProgressBar<Color>` | Progress bar, sta=color |
| `pbari0` | `ProgressBar<Image>` | Progress bar, sta=image |
| `slid0` | `Slider` | Slider |
| `gauge0` | `Gauge` | Gauge |
| `combo0` | `ComboBox` | ComboBox |
| `text0` | `Text` | Text |
| `stext0` | `ScrollText` | Scroll text |
| `btn0` | `Button` | Button |
| `dual0` | `DualStateButton` | Dual-state button |
| `num0` | `Number` | Number |
| `xf0` | `XFloat` | XFloat |
| `chk0` | `Checkbox` | Checkbox |
| `rad0` | `Radio` | Radio |
| `tsw0` | `ToggleSwitch` | Toggle switch |

Разместите виджеты без перекрытия критичных зон (достаточно минимальных размеров).  
**Touch:** у интерактивных — *Send Component ID*; id на панели 1…22 (авторегистрация в прошивке).

## Сборка и запуск

```bash
pio run -e example5
```

Страница в HMI: **id=0**, имя **`ex5`**. После `rest` прошивка ждёт 3 с и ставит команды в очередь (см. отладочный UART при `-DNEX_DEBUG`).

## Код

- `app.hpp` — страница и `AllComponentsDemoApp`
- `demo_controls.hpp` — `demoTimer`, `demoButton`, … по одному на виджет

## Не в библиотеке

TouchCap (type 5), PageComponent (type 121) — листовых классов в `nexComponents.hpp` нет.
