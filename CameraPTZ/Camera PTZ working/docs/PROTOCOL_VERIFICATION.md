# Сверка с ConvertibleProtocol.pdf v3.05

## Архитектура каталога

| Слой | Назначение |
|------|------------|
| `command_ids.hpp` | Последовательные `enum` (`CameraCmd`, `PtCmd`, `OsdItem`, …) — индекс в таблицу |
| `*_commands.hpp`, `*_menu.hpp` | `kCameraCommands[]`, `kOsdMenu[]`, … — subcode + имя из PDF |
| `menu_catalog.hpp` | `subcode(OsdItem)`, `getVideoMenu()` — доступ без строк |
| `CameraDeviceBase` / `PtDeviceBase` | `apply(CameraCmd)`, `send(PtCmd)`, `setOsd(OsdItem)` |

**Subcode в кадре:** два **hex**-символа ASCII (`0x70` → `QSA:70`), не десятичное «70».

## kSceneCommand (Pattern 3)

```cpp
// camera_commands.hpp
kSceneCommand = { "Scene File Select", "QSF", "XSF", CameraPattern::Scene };
```

- PDF: `[STX]XSF:n[ETX]` — выбор сцены.
- `OSF` / `CameraCmd::SceneFile` — запись номера scene file (Pattern 2).

## Исправления по PDF (2025)

| Было | PDF | Стало |
|------|-----|--------|
| `VideoMenuItem::SuperGain` @ `0x70` | QSA:70 = **SCAN REVERSE** | `ModeAtSuperGain` @ `0x60` (MODE @S.GAIN) для `setSuperGain()` |
| `VideoMenuItem::Reverse` @ `0x71` | QSA:71 = FRAME RATE RANGE | `ScanReverse` @ `0x70`, `FrameRateRangeVariable` @ `0x71` |
| `ExtendedJItem::SuperGain` @ OSJ:28 | Super Gain в Video/OSG | `VideoMenuItem::ModeAtSuperGain` |
| ATW через OSJ:25 | QSD:72 ATW SPEED | `OsdItem::AtwSpeed` |
| HDR через OSJ:23 | QSJ:26 HDMI Out HDR | `ExtendedJItem::HdmiOutHdrOutputSelect` |

## Проверка

```bash
python tools/verify_protocol.py
pio run -e ptz_panel
```

Каталог библиотеки: `lib/ccam/` (`include/{transport,protocol,catalog,devices}`, `src/`, `examples/`).

Скрипт проверяет: `static_assert` enum↔таблица, spot-check токенов в PDF, отсутствие `strcmp`/`ptFindCommand` в `src/` и `include/`.

## Ограничения

- В каталоге не все пункты PDF (сотни строк на стр. 6–27); только используемые + справочные OSD/Video.
- Колонки поддержки по модели (HE130, E600, …) в ROM не хранятся — проверка «поддерживает ли камера» остаётся на уровне модельных классов.
