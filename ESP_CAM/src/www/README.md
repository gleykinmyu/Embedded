# ESP_CAM Web UI

Исходники модульные; в прошивку уходит `generated/status.html` через
[`pio_idf_embed.cmake`](../../../libs/cmake/pio_idf_embed.cmake)
(PIO-safe аналог `EMBED_TXTFILES`) → `_binary_status_html_start/end`.

## Структура

| путь | роль |
|------|------|
| `index.html` | каркас, вкладки, placeholders |
| `panels/*.html` | Control / Camera / Paint / System |
| `css/app.css` | PTZ-pads, пресеты, sticky note |
| `js/app.js` | логика управления |
| `vendor/iptv-ui/kit/` | IPTV UI kit |
| `status.html` | **сборка** (`tools/build_www.py`) — превью в дереве |

## Сборка

```bash
python tools/build_www.py
python tools/build_www.py --out src/generated/status.html
```

CMake: `build_www.py` → `pio_idf_embed_txt` → линковка. Подробности уклада
PIO+IDF: [`libs/cmake/README.md`](../../../libs/cmake/README.md).

Диапазоны shutter/gamma/AGC/… — в `ccam` (`cam_param_ranges`), UI:
`/api/cam?cmd=ranges`.
