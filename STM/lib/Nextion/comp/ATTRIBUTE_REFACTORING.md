# План рефакторинга атрибутов компонентов

Живой план для `lib/Nextion/comp/` и `lib/Nextion/comp/resources/`.  
Отмечайте: `[x]` сделано, `[~]` в работе, `[ ]` запланировано, `[-]` отменено.

Связанный общий backlog: [`../REFACTORING.md`](../REFACTORING.md).

---

## Цели

1. **Разделить роли атрибутов** по комментариям `user` / `mcu` в коде и в HMI.
2. **MCU numeric (host → panel):** только исходящий `assign`, без зеркала и без `get`/`fetch` с панели.
3. **User + RO + строки:** оставить привычные поля `attr::Num` / `attr::String` / `attr::NumRO` с `operator=` / `.set()` / `applyResponse`.
4. **Читаемый прикладной API:** `enable()`, `setPeriod()`, `font.setTextColor()` вместо сырых имён NIS (`en`, `pco`, `vvs0`).
5. **Группировка** повторяющихся NIS-полей в `resources::*` (`Font`, `Background`, `Cursor`, …).

---

## Целевая модель (как сейчас задумано)

| Категория | Доступ из прикладного кода | Зеркало на MCU | Запрос `get` с панели |
|-----------|---------------------------|----------------|------------------------|
| **user** | публичное поле `attr::Num` / `attr::String` | да (`applyResponse`) | через поле `.get()` при необходимости |
| **mcu numeric** | метод `set…()` / `enable…()` | нет | нет |
| **mcu string** | публичное поле `attr::String` + `.set()` | да | через `.get()` при необходимости |
| **RO** (`w`, `h`, `qty`) | публичное поле `attr::NumRO` | да | через `.get()` |
| **x, y** | публичное поле `attr::Num` (исключение: при `drag` может меняться с панели) | да | опционально |

### Техническая база MCU numeric

```cpp
// nexAttributes.hpp — attr_detail::
assignNumeric(parent, attrName, tag, value);
// → Transaction{ cmd::assign::Numeric, page.ID, id(), tag }
```

- Обёртки `McuNum` / `McuString` **не используются** (удалены).
- Ресурсы хранят `Component& owner` и вызывают `assignNumeric` из методов.

### Bool-режимы (явное действие)

Пара методов вместо `setX(bool)`:

| Было | Стало |
|------|--------|
| `setEn` (Timer) | `enable()` / `disable()` |
| `setEn` (ScrollText) | `enableAutoScroll()` / `disableAutoScroll()` |
| `setDrag` | `enableDrag()` / `disableDrag()` |
| `setWordWrap` | `enableWordWrap()` / `disableWordWrap()` |
| `setKey` | `bindKeyboard()` / `unbindKeyboard()` |
| `setPw` | `enablePassword()` / `disablePassword()` |
| `setYcen` / `setXcen` (ComboBox) | `enableVerticalCenter()` / …Horizontal…` |

Команды NIS на виджете (не атрибуты): `refresh()`, `show()`/`hide()`, `move()`, `click()`, `touchSwitch()` — без изменений.

---

## Файлы

| Файл | Роль |
|------|------|
| `nexAttributes.hpp` | `attr::Num`, `attr::String`, `attr_detail::assignNumeric` |
| `nexCompImpl.hpp` | Базы: TouchArea, Drawable, Styled, Printable, Multiline, Numeric, … |
| `nexCompImpl.cpp` | `onResponse` для x/y/w/h, drag (без зеркала drag) |
| `nexComponents.hpp` | Листья виджетов |
| `resources/background.hpp` | Фон по `BGStyle`, ProgressBarBackground, Background2 |
| `resources/font.hpp` | font / pco / spax |
| `resources/cursor.hpp` | Slider thumb |
| `resources/floatPoint.hpp` | XFloat vvs0/vvs1 |
| `resources/pressed.hpp` | bco2/pco2 нажатого состояния |
| `nexComponentBase.hpp` | enum `Type`, дерево наследования (комментарии) |

---

## Выполнено

- [x] `namespace attr::Num` / `String` / `NumRO` вместо отдельных классов-обёрток.
- [x] `resources::Background<S>`, `Font`, `Pressed`, группировка полей NIS.
- [x] `enum Tag` внутри каждого класса / ресурса.
- [x] Условная компиляция TouchArea Position/Size, Drawable drag/aph/effect.
- [x] Удалён `nexAttrMcu.hpp`; MCU numeric → `assignNumeric` + только `set*`.
- [x] Строки и RO не переводились на «только методы».
- [x] Первая волна action-имён: `enable`, `bindKeyboard`, `enableDrag`, …
- [x] XFloat: `resources::FloatPoint point` с `setLeft` / `setRight` (пока короткие имена).

---

## Фаза A — Имена «обрывков NIS» (высокий приоритет)

Одинаковые имена NIS в разных виджетах → **разные методы по смыслу UI**.

### A.1 `dis`, `tim`, `wid`, `hig`

| Класс | NIS | Сейчас | Предложение |
|-------|-----|--------|-------------|
| Timer | `tim` | `setTim` | `setPeriod()` |
| ScrollText | `tim` | `setTim` | `setScrollPeriod()` |
| ScrollText | `dis` | `setDis` | `setScrollStep()` |
| QRCode | `dis` | `setDis` | `setScale()` |
| ListSelect | `dis` | `setDis` | `setItemSpacing()` |
| ListSelect | `hig` | `setHig` | `setRowHeight()` |
| ComboBox | `dis` | `setDis` | `setItemSpacing()` |
| ComboBox | `wid` | `setWid` | `setDropDownWidth()` |
| DataFile | `dis` | `setDis` | `setCellSpacing()` |
| ToggleSwitch | `dis` | `setDis` | `setLabelGap()` |
| Waveform | `wid`/`hig` | `setWid`/`setHig` | `setPlotWidth()` / `setPlotHeight()` |
| Gauge | `hig` | `setHig` | `setScaleHeight()` |

### A.2 Списки и числа

| Класс | Сейчас | Предложение |
|-------|--------|-------------|
| ComboBox | `setQty` | `setRowCount()` |
| ComboBox | `setVvs0` / `setVvs1` | `setListOffset()` / `setListOffset2()` |
| ComboBox | `setMode` | `setListMode()` |
| Number | `setLength` | `setDigitCount()` |
| FloatPoint | `setLeft` / `setRight` | `setDigitsBeforePoint()` / `setDigitsAfterPoint()` |
| ProgressBar (image) | `setValue` | `setFillLevel()` |
| Gauge | `setVal` | `setNeedleValue()` |
| Waveform | `setCh` | `setChannel()` |

### A.3 Выравнивание (Multiline)

| Сейчас | Предложение (опционально) |
|--------|---------------------------|
| `setVAlign` | `setVerticalAlign()` |
| `setHAlign` | `setHorizontalAlign()` |
| Numeric | `setFormat` | `setNumberFormat()` |
| Selection | `setColor` | `setMarkerColor()` |

**Критерий готовности фазы A:** в `examples/` и прикладном коде нет неоднозначных `setDis`/`setTim` без префикса виджета; grep по старым именам — пусто.

---

## Фаза B — `resources::*` (средний приоритет)

Единый стиль: доменное имя + роль, не `setW` / `setColor` без контекста.

### Font

| Сейчас | Предложение |
|--------|-------------|
| `setId` | `setFontId()` |
| `setColor` | `setTextColor()` |
| `setSpacing` | `setCharSpacing()` |

### Cursor (Slider)

| Сейчас | Предложение |
|--------|-------------|
| `setW` / `setH` | `setWidth()` / `setHeight()` |
| `setColor` | `setThumbColor()` |

### Background

| Сейчас | Предложение |
|--------|-------------|
| `setColor` | `setBackgroundColor()` (или оставить `setColor` внутри `bg`) |
| `setImage` | `setBackgroundImage()` |
| `setCrop` | `setBackgroundCrop()` |
| `setBpic` | `setBarBackgroundImage()` |
| `setPic1` / `setPicc1` | `setSecondaryImage()` / `setSecondaryCrop()` |

### Pressed

| Сейчас | Предложение |
|--------|-------------|
| `setColor` | `setPressedTextColor()` |

**Критерий:** вызовы вида `widget.font.setTextColor(c)` читаются без знания NIS.

---

## Фаза C — Drawable и визуал (низкий/средний приоритет)

| Сейчас | Предложение | Примечание |
|--------|-------------|------------|
| `setAph` | `setOpacity()` | 0…127 |
| `setEffect` | `setTransitionEffect()` | |
| `setLayer` | `placeAbove()` | рядом с `show()`/`hide()` |
| `visible(bool)` | оставить или только `show`/`hide` | breaking, если убрать |

### Gauge / картинки

| Сейчас | Предложение |
|--------|-------------|
| `setPicUp` / `Down` / `Left` | `setUpImage()` / `setDownImage()` / `setLeftImage()` |
| `setPco` / `setPco2` | `setScaleColor()` / `setAccentColor()` |

### ToggleSwitch

| Сейчас | Предложение |
|--------|-------------|
| `setBco2` / `setPco2` / `setPco1` | `setTrackColor()` / `setOffColor()` / `setOnColor()` |
| `setFont` | `setLabelFontId()` |

---

## Фаза D — DataFile / FileBrowser (когда появятся листья)

| Сейчас | Предложение |
|--------|-------------|
| `setMaxvalY` / `setMaxvalX` | `setScrollLimitY()` / `setScrollLimitX()` |
| `setBco2` / `setPco2` | `setAltBackgroundColor()` / `setAltTextColor()` |

---

## Фаза E — Документация и дерево (поддержка)

- [ ] Обновить ASCII-дерево в `nexComponentBase.hpp` под методы (`// setPeriod`, не `// tim`).
- [ ] Легенда в `nexAttributes.hpp` синхронизировать с финальными соглашениями.
- [ ] Краткая таблица «виджет → MCU API» в `examples/` или в этом файле (приложение).

---

## Соглашения об именовании (целевые)

1. **Действие вкл/выкл:** `enableX()` / `disableX()`, не `setX(bool)`.
2. **Число как параметр:** `set<Role><Unit>()` — `setPeriod`, `setDigitCount`, `setFillLevel`.
3. **Размеры:** `setWidth()` / `setHeight()`, не `setWid` / `setHig`.
4. **Цвета:** `set<Text|Thumb|Marker|Bar>Color()`, не голый `setPco` в листьях (в `resources` допустим префикс роли).
5. **Картинки:** `set<Role>Image(PicId)`.
6. **Строки user/mcu:** поле + `txt.set("…")` без обязательного `setTxt()` на классе.
7. **Не дублировать** один метод на разных уровнях иерархии с разным смыслом (`setDis` на ScrollText ≠ ComboBox).

---

## Порядок работ (рекомендуемый)

```text
1. Фаза A.1 — dis/tim/wid/hig (самый большой выигрыш)
2. Фаза A.2 — qty, vvs, length, float point, progress/gauge
3. Фаза B   — resources (мало вызовов, локальный diff)
4. Фаза C   — drawable + gauge images + toggle colors
5. Фаза D   — data/file виджеты
6. Фаза E   — комментарии и примеры
```

После каждой подфазы:

- `grep` старых имён в `lib/Nextion/examples/` и в репозитории;
- сборка `example2` / `example4`;
- при необходимости — alias deprecated на одну версию (только если нужна мягкая миграция; по умолчанию **жёсткая замена**).

---

## Миграция прикладного кода (шпаргалка)

| Было | Стало (уже) | Станет (фаза A/B) |
|------|-------------|-------------------|
| `timer.tim = 100` | `timer.setTim(100)` | `timer.setPeriod(100)` |
| `timer.en = 1` | `timer.enable()` | — |
| `slider.drag = 1` | `slider.enableDrag()` | — |
| `text.key = 1` | `text.bindKeyboard()` | — |
| `xf.vvs0 = 3` | `xf.point.setLeft(3)` | `xf.point.setDigitsBeforePoint(3)` |
| `combo.txt.set("x")` | без изменений | без изменений |
| `btn.val = 1` | без изменений (user) | без изменений |

---

## Вне scope (пока не трогать)

- Публичный API `Application` (`setBrightness`, facades).
- `nexMsgBox`, `nexCanvas` — отдельные сущности.
- Автогенерация тегов / полей из NIS JSON.
- Зеркало для MCU numeric и синхронизация `get` после `assign` (осознанно не делаем).
- Перевод **всех** mcu-строк на «только методы» (остаются `attr::String`).

---

## Риски

| Риск | Митигация |
|------|-----------|
| Breaking rename в примерах/прошивке | grep + чеклист в этом файле |
| Путаница `setValue` (Slider user vs ProgressBar mcu) | разные имена: поле `value` vs `setFillLevel()` |
| Дубли имён на наследниках | переименовывать на **листе**, не в базе с разным смыслом |
| `onResponse` без зеркала для mcu | не полагаться на чтение mcu-полей после touch; user/RO — как раньше |

---

## Чеклист перед закрытием всего плана

- [ ] Все пункты фаз A–C отмечены или явно `[-]`
- [ ] Examples обновлены
- [ ] `ATTRIBUTE_REFACTORING.md` и дерево в `nexComponentBase.hpp` согласованы
- [ ] Нет ссылок на `nexAttrMcu.hpp` / `McuNum` / `fetch*` для mcu-методов

---

*Последнее обновление плана: 2026-05-31.*
