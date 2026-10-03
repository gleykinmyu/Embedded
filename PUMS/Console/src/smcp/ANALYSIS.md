# SMCP — что переделать

30 сентября 2026. Контракт кадра — `PROTOCOL.md`. Экран пульта — `src/Console/UI/TODO.md`. Здесь только дыры модели и шины.

## Файл / FIO (решения + TODO)

Принято при разборе multi-server / SectionPool (A1–A4). Реализацию не делать в этом же проходе — сначала закрыть остальные дыры списка.

### Решения

- **A1.** Один live `Show` + staging `Show` (как сейчас у FIO). Тип файла = набор секций (SERV/GRUP/PRST/SETT…). Load/save — **чекбоксы по секциям**; пустые = no-op. «Чистый патч» / «чистое шоу» — какие секции выбраны/есть, не два класса Show.
- **A2.** Load: GRUP/PRST `seg_id` ∉ SERV → **предупреждение**, файл принимаем. Слоты на отсутствующий seg — тот же UI, но recall/record **disarm** до правки патча + save. Данные из файла не чистить.
- **A3.** Пока две копии Show (live + incoming). Overflow ёмкости → MsgBox «Загрузить частично?» (факты: секция, headers/slots found vs max); Yes = truncate, No = секцию не трогать. Потолки `{tag, max_count}` — таблица в прошивке (числа позже).
- **A4.** Носители и RAM:
  - **Рабочая копия всегда в RAM** (`Show` live). С W25/SD **не** редактируем in-place.
  - **Load:** прочитать слот/файл → буфер → разобрать в live. **Save:** сериализовать live → записать.
  - **W25 A/B** (2×4K, ping-pong, seq+CRC): защищает LKG при **обрыве питания во время Save на W25**. Реализуется **в драйвере / `W25qShowFile`**, не в `sf::Fio` — для FIO это обычный `IFile` (bak/mirror). Битый load с SD → FIO не трогает bak, откат live из W25.
  - **Нет SD:** только «Сохранить» (не «как…») → W25; пометить UI/state **«только W25Q»**. **Появилась SD** → предложить сохранить текущий файл на карту.
  - **Есть SD:** Save → SD (+ обновить W25 LKG тем же образом).
  - Большой файл / стрим с W25 — H1 later; пока укладываемся в бюджет сектора(ов) или только LKG-урезание.
- **A5.** Обратная совместимость / миграция старого GRUP — **не делаем**, пока нет прода и полевых файлов. Пишем только новый формат; `Header.version` не разветвляем.
- **A6.** `markEdited` / dirty — **на секцию (или пул)**, не один флаг на весь Show. UI `*` и save смотрят маску dirty; save = чекбоксы ∩ dirty (или явный override чекбоксами). Load clear dirty у принятых секций.

### TODO — переформатировать `sf::Fio` / W25 mirror

Когда дойдём до реализации (после закрытия дизайн-дыр A…):

1. Load/save по маске секций (чекбоксы), не «весь файл целиком».
2. Accept с предупреждениями A2 + disarm слотов; revalidate после save патча.
3. Overflow-диалог A3; без тихого truncate.
4. `W25qShowFile` (не Fio): A/B два сектора, commit после program; open читает активный.
5. Fio: bak = этот IFile; после успешного SD-save — sync bak; bad SD load — не писать bak, restore с bak.
6. Нет SD: Save → только bak + флаг «только W25Q»; SD появилась → предложить save на карту.
7. Per-section / per-pool `edited` (A6); UI `*` и save по маске dirty.

## Адресация / сегменты (решения)

- **B1.** На пульте **нет** `mech[id]` через `storage(server_id)` / ObjRegistry. Есть `CMechBank` — слоты, которые **заполняются по SERV** (субсекции механизмов). Слот = оперативка (live) + ссылка на config в SERV. `storage(server_id)` у консоли убираем; lookup — банк слотов + правила SERV. (На сервере свой `storage()` для приводов — отдельно.)
- **B2.** Gaps `mech_id` **разрешены**, id не уплотняем. Нет в SERV → пустая клетка; бит на отсутствующий id → MechNotFound / вырезать при record.
- **B3.** Цепь/трос: **одна консоль**; в прошивке **сервера** — два `Node` на один CAN (два logical `server_id` / seg). Пульт видит два peer-сессии. Jack→seg — на сервере при сборке inventory.
- **B4.** **CanDemux** — спец-драйвер: один `ICAN` → два `Node`; RX demux по `dst`; TX в общий CAN. Не один Node на два ILink.
- **B5.** Вместо `Mute`: из списка **обычных** групп выбираем одну «текущего шоу»; её **номер** — в спецполе (SETT). Пишем/record в неё как в любую. UI-фильтр «только используемое» смотрит эту группу. Не отдельный тип GRUP.
- **B6.** ~~Basic server/mechs шаблон~~ — **не нужно**: патч/profile приходит **с сервера** (хэш + bulk). Offline без SERV — только load файла с SERV (§3), не выдумывать basic.

## Группы (решения)

- **C1.** `smcp::test` / local selection (`group_pool.hpp`) — **старый код**, не база leaf. Цель: wire-слоты `(seg_id, Selection)` в пуле; local→server через CMech не тащить.
- **C2 / C4.** Partial multi-seg: **best-effort OK** (частичный Select и частичная уставка пресета) + отчёт Nack. **Atomic** → откат успешного при любом Nack.
- **C3.** Blocked GRUP vs Block шины — **как сейчас**: recall/trySelect смотрят оба; клетка → PDU `Block`; группа → только флаг GRUP.
- **C5.** Isolate / Спектакль — **UI**: в этих режимах лишний Select просто не вызывается. Guard в `trySelect`/`recall`/`IConsole::select` не обязателен.

## Пресеты (решения)

- **D1.** Слот пресета в пуле: `{ seg_id, Selection, MotionTarget }`. Несколько seg → несколько слотов на один header.
- **D2.** Recall пресета: сначала **все Select**, дождаться **Ack по всем**; только потом слать **SetTarget**. Без Select / Nack на Select → Target не слать (Busy / отчёт).
- **D3.** Overlap оси в слотах одного пресета — **проверка при record и при load** (reject / isValid false). Отдельной «починки» нет.
- **D4.** Partial = **C2** (best-effort / Atomic-откат на фазе Select). Уставки — только после полного успеха Select-фазы (D2).

## Протокол / multi-console (решения)

- **E1.** `acceptSelect` / SelectLimit — **оставить**. Политика Block при >1 пульта — позже.
- **E2.** `console_id` — **сохранять** (W25 / backup RTC), читать до `begin`; смена → `begin` + `start`. Не в шоуфайле.
- **E3.** Drive / motion — **внутри сервера** (UART к лебёдкам и т.п.), не дыра пульта/шоу. Пульт шлёт SetTarget; как сервер двигает ось — его дело.
- **E4.** `ResetFault` — **отдельное сообщение (MsgId)**. Пульт шлёт после **снятия** кнопки E-Stop (release), не в момент нажатия.
- **E5.** Линк к платам/приводу (CAN2 / UART / …) — **только сервер**. Пульт не знает.

## UI (решения)

- **F1.** Контекст seg — **UI** (в PUMS несколько страниц). Сейчас не фиксируем раскладку; модель даёт seg/слоты.
- **F2.** Нет связи / нет оси → клетка Disabled, высота «—». **Как сейчас.**
- **F3.** Имя сегмента — `name[]` в SegHdr (SERV) для UI.
- **F3b / inventory sync.** **Без VID/UID.** Последовательность — ниже «Multi-byte / config». `server_id` — только адрес сессии.

### Multi-byte / config (MsgId + механизм)

Последовательности § connect / broadcast / WriteConfig — приняты. Ниже wire.

**Доп. northbound MsgId (черновик `CMsgId`, класс PROTOCOL A–E):**

| MsgId | Имя | Класс | Body (≤8 B) | Назначение |
|-------|-----|-------|-------------|------------|
| 0x22 | `ResetFault` | A | mask/selection или mech | после release E-Stop (E4) |
| 0x30 | `GetConfigHash` | A | `type:u8` | запрос хэша |
| 0x31 | `ConfigHash` | E | `type:u8` + `hash:u32` LE | ответ / текущий хэш |
| 0x32 | `ConfigHashChanged` | D | `type:u8` + `hash:u32` | broadcast с сервера |
| 0x33 | `GetConfig` | A | `type:u8` | начать выгрузку blob |
| 0x34 | `ConfigInfo` | E | `type:u8` + `total:u16` + `crc16:u16` | размер + CRC всего blob |
| 0x35 | `GetConfigChunk` | A | `type:u8` + `offset:u16` + `max:u8` | pull куска |
| 0x36 | `ConfigChunk` | E | `offset:u16` + `data[6]` | кусок (до 6 B) |
| 0x37 | `PutConfig` | A | как ConfigInfo | сервис: начать запись на сервер |
| 0x38 | `PutConfigChunk` | A | `offset:u16` + `data[6]` | сервис: кусок → Ack/Nack |
| 0x39 | `PutConfigCommit` | A | `type:u8` + `crc16:u16` | сервис: применить; сервер → broadcast 0x32 |

`type`: пока `ServProfile = 1` (blob паспортов/SERV). Позже ShowBlob для пульт↔пульт.

**Механизм (pull с сервера — надёжнее на CAN):**

1. `GetConfigHash` → Ack → `ConfigHash`. Сверка с local.  
2. Mismatch → `GetConfig` → Ack → `ConfigInfo(total, crc16)`.  
3. Цикл: `GetConfigChunk(offset, max≤6)` → Ack → `ConfigChunk` → копировать в буфер; `offset += len`.  
4. `offset >= total` → проверить crc16 → принять в live SERV / сохранить хэш.  
5. Обрыв HbLost / Nack → abort, буфер отбросить; LKG/profile не портить.

**WriteConfig (сервис):** `PutConfig` → цикл `PutConfigChunk` (каждый class A + Ack) → `PutConfigCommit` → сервер новый хэш + `ConfigHashChanged`.

**Не multi-byte:** Select/Block/SetTarget/Telemetry/HB — как сейчас.  
**Fio** в этом не участвует; blob → RAM SERV, потом обычный Save секции.

### Сервис / PIN (совет, MVP)

У больших пультов (grandMA и т.п.) — **пользователи и права на консоли/в шоу**, не на каждой лебёдке. Это про программирование спектакля, не про калибровку шкафа.

У нас опасная операция — **PutConfig на сервер**. Значит:

| | MVP | Позже (большие консоли) |
|--|-----|-------------------------|
| Кто хранит секрет | **сервер** (NVM): один service PIN, хэш (не plaintext) | роли/юзеры на **пульте** (как MA: Admin/Program/Playback) |
| Кто пускает PutConfig | **сервер** отвергает 0x37–0x39 без unlock | + UI пульта прячет сервис |
| Unlock | `ServiceUnlock` (A) с PIN → сессия «service» до timeout / HbLost / Lock | login user на консоли |
| Шифрование шины | **нет** в MVP (закрытый CAN площадки) | если выйдем в чужую сеть — отдельно |
| Юзеры шоу на каждой консоли | не плодить | да, локально / sync шоу пульт↔пульт |

**Не делать сейчас:** БД пользователей на каждом сервере и на каждой консоли сразу; AES паролей на CAN; «как Active Directory».

**Делать:** один PIN на сервер + unlock на сессию; пульт только UI. Пользователи спектакля — когда дойдут большие консоли, хранить **на пульте** (в шоу/NVM пульта), не на приводном сервере.

## MechConfig / типы (решения)

- **G1.** В **MotionTarget — условные единицы** `kind` (signed int, без `_mm` в имени) + **тип хода Absolute | Relative**. Continuous — см. storage §10.
- **G3.** FlightObject — **супер позже**, большие консоли; не kind оси, не MVP.
- **G4.** Load scale — уже в MechConfig (`LoadUnit` + `load_scale_*`).

## Инфра (решения)

- **H1.** RAM: сначала потолки `{tag, max}` (A3); МК F427→F767→H7 — когда упрёмся. Возможно **чтение шоу с W25Q** (не держать всё в RAM / стрим секций) — учесть вместе с A4 A/B LKG и TODO FIO.
- **H2.** `MaxSessions ≥ MaxSeg`; `start` на каждый `server_id` из патча. Сейчас 1 слот — для цепи/троса мало.

### TODO — ответственность хранения: сервер vs пульт vs шоу (разобрать отдельно)

Контекст: несколько пультов должны видеть **один** механизм; декодер/ноль — не локальная правда каждого SD.

**Черновое разделение (не реализация, только зафиксировать мысль):**

| Данные | Владелец | Заметки |
|--------|----------|---------|
| scale, home/zero, hard limits, `kind` | **сервер** | паспорт оси / commissioning; пульт не «держит ноль» |
| live: position, holder, block, fault | **сервер → Telemetry** | все пульты видят одно |
| GRUP / PRST / SETT | **шоуфайл на пульте** | пока нет общего show-server; обмен = файл |
| MotionTarget в пресете | пульт (числа) / сервер (исполнение) | единицы = те, что объявил сервер |
| MechConfig в SERV на SD | ? зеркало/кэш для UI / offline | не источник истины при online |

**Открытые вопросы (надо решить до кодирования inventory):**

1. **Commissioning flow:** **вариант C** — обычный режим только читает; PutConfig — сервис.  
   **Доступ (рекомендация, не полный IAM):** см. ниже «Сервис / PIN».
2. **Как пульт узнаёт паспорт оси:** сначала **хэш настроек** (сверка с profile); при нужде — выгрузка структур через **multi-byte transfer по Node** (не VID/UID).
3. **Один тип файла (SMCP), секции чекбоксами (A1).** Profile/патч = секция SERV в том же файле (и/или после bulk с сервера в live). **Правило:** если на пульте в live **нет SERV** — нельзя грузить отдельно только GRUP/PRST/SETT; нужен файл с SERV или **bulk с сервера**. Есть SERV → можно догружать шоу-секции.
4. **Смена настроек на сервере:** сервер шлёт **broadcast-уведомление** (hash/config changed). Пульты предлагают/забирают bulk → обновляют profile/UI. **Пресеты/группы по шине не авто-править.**
5. **Совместимость пресета после смены паспорта (после broadcast + bulk):**  
   - home/scale — только сервисный режим; штатному пульту всё равно; числа PRST = signed int от нуля.  
   - limits / нет оси → runtime **Nack** с сервера; нет seg → ошибка консоли (A2). Отдельно PRST не инвалидировать заранее обязательно.  
   - **Invert** — дело сервера; пульту для Select/Target всё равно (в profile может быть для UI).  
   - **`kind` (тип оси)** — единственное критичное для пространства чисел: при смене profile **проверить** mech_id, у которых kind изменился → warn + disarm затронутых PRST/GRUP слотов; файл не чистить. (Смена kind — «не должно случаться»; проверка на случай сервиса.)  
   - Fingerprint в каждом слоте PRST — не нужен.
6. **Синхрон шоуфайла между пультами:** **вариант B** — в multi-byte сразу `type=ShowBlob` (те же chunk MsgId); сервер ни при чём; сессия пульт↔пульт. UI «отправить/принять шоу» — с появлением второго пульта. Live collaborative (**C**) — надстройка позже (не блокер; по оценке — несложно поверх B).
7. **~~B6 basic~~** — снято: конфиг **запрашиваем с сервера** (F3b).
8. **Mute → B5** (группа шоу + номер в SETT). Имена seg/осей — из profile с сервера (и SegHdr в SERV при save); в шоу не дублировать как истину.
9. **~~VID/UID~~** → хэш настроек + bulk transfer структур по Node (F3b).
10. **Continuous / тип движения:** в **SetTarget / MotionTarget** — тип **Absolute | Relative** (для всех осей, не только continuous).  
    Непрерывное вращение: паспорт оси с **min/max на краях диапазона** (полные пределы int / «нет упора») ⇒ сервер трактует как continuous (рядом с `MechFlag::Continuous` / без soft-hard).  
    Пресет хранит те же числа + тип хода; отдельной семантики «delta-only пресет» нет.

**Чего точно не делать вслепую:** копировать весь MechConfig в каждый слот пресета; слать по CAN «поправь свои пресеты» на другие пульты.

### 1. Номер пульта

`begin(console_id)` есть. Id после включения неоткуда взять, повторного `begin` после IdConflict с новым номером нет.

Сейчас константа `1`: `CanLink(board.can, 1u)` и `console.begin(kConsoleIdMin)` в `Console/main.cpp`, то же в `main_test.cpp`.

Прочитать id до `CanLink` / `begin`. Хранение и страница настроек — `UI/TODO.md` §2. После смены id leaf снова вызывает `start(server)`.

### 2. Isolate и режим Спектакль

`isolateGroup` — SETT шоуфайла, на сервере флага нет. Сейчас глушит только Press клетки. `CGroup::recall` и `CGMech::trySelect` isolate не смотрят.

`MConsole::Mode::Show` гасит меню. `onCellPress` / `recall` mode не проверяют — Select уходит.

- isolate — в `trySelect` и `recall`;
- Спектакль — запрет Select в `trySelect` / `recall` / `IConsole::select`.

Guard страниц от HMI `page` по UART — план UI §5.

### 3. Block и ширина маски

Клетка в режиме Block шлёт сегментный PDU `Block`. Флаг Blocked в GRUP — локальный, сервер его не знает.

`acceptBlock` в базе — Ok любой консоли `0x01…0x0F`. `Action::Set` с пустой маской снимает Block со всего сегмента. Кто имеет право слать Block — задать в leaf, когда пультов больше одного.

Жест клетки и группы — `UI/TODO.md` §6. Сегментный Block не делать следствием нажатия клетки.

Маска `Selection` — 32 бита. Inventory пульта и сервера — 24. Биты 24…31 сервер ответит `MechNotFound`. Резать маску по `mechCapacity()` либо поднять inventory до 32.

### 4. Привод

`DriveMech::setTarget` пустой: Ack и Telemetry уходят, ось стоит.

- Moving и позиция, либо отказ до Ack.
- `acceptSetTarget`: концевики и ход → `Limits`. Сейчас Ok всем.
- `accel_mm_s2` на шину не кладётся и при разборе обнуляется. Класть в кадр либо убрать из цели.
- `resetFault()` пустой, MsgId нет. Кнопку сброса не включать раньше PDU.
- Телеметрия хода — Δposition / rate-limit в тике привода, `pushTelemetry`. Базовый `IServer` шлёт снимок при смене select / block / SetTarget / GetTelemetry / HbLost, не при движении.

Пока привод пустой — поездку на экране не делать (`UI/TODO.md`).

### 5. CAN2

Второй `Node` на сервере: CAN1 — консоли, CAN2 — платы. Свой PDU привода, не расширение транспортного `message.hpp`. `Select` / `Block` / holder на CAN2 не тащить. `DriveMech` мапит плату на `IMech`. Пульт видит «ось N на `server_id`».

CAN2 на F407 закрыт: в `board` только CAN1. Один `Node` на два `ILink` не делать.

### 6. Второй сегмент

Второй сервер: в `MConsole::storage` вернуть второй банк. `serverIdForGlobalMech` / `mechIdForGlobalMech` нигде не вызываются.

Второй слот сессии — `GroupConsole<24>` сменить на `MaxSessions > 1` и расширить `SessionBank` в `MConsole`. Свободного слота под второго peer нет.
