# Embedded: PIO + ESP-IDF compat

Для проектов с `framework = espidf`. PlatformIO остаётся каталогом железа
(env, upload, OTA, monitor); сборка — штатный IDF CMake. Здесь только
обходы стыка с SCons.

## Роли

| Слой | Источник правды |
|------|-----------------|
| `platformio.ini` | board, flash, partitions, upload/OTA |
| IDF `CMakeLists.txt` | SRCS, REQUIRES, components |
| `libs/cmake/*` + `tools/*` | embed, board select, compiledb |

## Нельзя / чем заменить

| Под чистым IDF | Под PlatformIO |
|----------------|----------------|
| `EMBED_TXTFILES` / `EMBED_FILES` | [`pio_idf_embed.cmake`](pio_idf_embed.cmake) |
| кастомный `build_dir` | дефолт `.pio/build/<env>` |
| `board_build.embed_txtfiles` | тот же cmake-helper |
| packages cache | junction `.pio_core_root` → `~/.platformio` (clangd), **не** `build_dir` |

## Embed

```cmake
include("${CMAKE_SOURCE_DIR}/../libs/cmake/pio_idf_embed.cmake")
pio_idf_embed_txt(UI_ASM "${CMAKE_CURRENT_LIST_DIR}/index.html")
idf_component_register(SRCS ... "${UI_ASM}" ...)

if(CMAKE_BUILD_EARLY_EXPANSION)
    return()
endif()
pio_idf_embed_track(${COMPONENT_LIB} "${UI_ASM}" "${CMAKE_CURRENT_LIST_DIR}/index.html")
```

Символы как у IDF: `_binary_<filename>_start/end` (`index.html` → `_binary_index_html_*`).

## House-rules (espidf)

1. Platform pin (pioarduino / IDF 5.3) — один на monorepo.
2. Не ставить `build_dir` вручную.
3. Multi-board: `select_board.py` → `APP_BOARD` + `SDKCONFIG_DEFAULTS`.
4. `extra_scripts`: только `pio_compiledb.py`, `select_board`, OTA fixup.
