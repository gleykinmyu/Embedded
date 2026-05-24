# Examples

Примеры не компилируются в состав библиотеки (`library.json` → `srcFilter: -<examples/**>`).

| Каталог | Описание |
|---------|----------|
| [ptz_panel](ptz_panel/) | HE130: джойстик + touch → `app::PtzOperator` |

## Сборка

Из корня репозитория:

```bash
pio run -e ptz_panel
```

Исходники примера: `lib/ccam/examples/ptz_panel/src/` (в т.ч. `src/app/` — только для демо, не часть API ccam).
