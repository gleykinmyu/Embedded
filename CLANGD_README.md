# Clangd в этом репозитории

Одна схема для Windows и macOS: тулчейны PlatformIO лежат у каждого пользователя в `~/.platformio`, и `compile_commands.json` ссылается на эту папку абсолютным путём.

## Как это устроено

В корне репозитория есть локальная ссылка `.pio_core_root`. Она смотрит на глобальную папку PlatformIO **этой** машины (`%USERPROFILE%\.platformio` или `~/.platformio`). Ссылка в Git не входит: она записана в `.gitignore`.

Clangd читает `compile_commands.json` в каталоге проекта (рядом с `platformio.ini`). Поле `directory` — абсолютный путь проекта на этой машине (`C:/GitHub/Embedded/ESP32`). Clangd 22 не находит файл, если там стоит `.`.

Пути к тулчейну и заголовкам ESP-IDF записаны как реальная папка `C:/Users/.../.platformio/...`, не как `.pio_core_root`. Windows при открытии файла раскрывает junction в этот путь, и иначе команда сборки не прикрепляется к `gpio.h`: пропадает сгенерированный `sdkconfig.h`.

Ключ `QueryDriver` внутри `.clangd` эта версия Clangd игнорирует. Один аргумент редактора покрывает все проекты, папки вручную не перечисляются: `--query-driver=**/xtensa-esp32*-elf-gcc*,**/arm-none-eabi-gcc*,**/avr-gcc*` (и те же маски для `g++`).

В каждом проекте свой `.clangd` с `CompilationDatabase: .` — база берётся из каталога проекта. Для заголовков из `~/.platformio` в настройках ESP32 дополнительно задан `--compile-commands-dir` на папку проекта.

`compile_commands.json` проектов в Git коммитятся. Каталоги сборки `.pio` по-прежнему не коммитятся.

## Одна команда после git clone

Её нужно выполнить **в корне репозитория** (там, где лежит этот файл), один раз на новой машине. PlatformIO на этой машине уже должен быть установлен, чтобы папка `~/.platformio` существовала.

Windows, командная строка `cmd`:

```bat
mklink /D .pio_core_root "%USERPROFILE%\.platformio"
```

Если Windows отвечает «Недостаточно привилегий», включите «Режим разработчика» (Параметры → Конфиденциальность и защита → Для разработчиков) и повторите команду. Без прав администратора ту же ссылку даёт junction — clangd ходит по нему так же:

```bat
mklink /J .pio_core_root "%USERPROFILE%\.platformio"
```

macOS и Linux, из корня репозитория:

```bash
ln -s ~/.platformio ./.pio_core_root
```

Проверка: `dir .pio_core_root\packages` (Windows) или `ls .pio_core_root/packages` показывает пакеты PlatformIO.

На текущей Windows-машине создан junction (`mklink /J`), потому что `mklink /D` был отклонён из-за прав.

## Новый проект PlatformIO

Из `tools/platformio-template` в проект копируются только файлы без абсолютных путей:

- `.vscode/settings.json` — Clangd читает `compile_commands.json` этой папки, IntelliSense C/C++ выключен, CMake при открытии папки не спрашивает набор инструментов. Файл локальный: строка есть в `gitignore.snippet`.
- `.vscode/extensions.json` — рекомендуются Clangd и PlatformIO. Его можно коммитить.
- `.clangd` — база компиляции берётся из каталога проекта.

`launch.json` и `c_cpp_properties.json` копировать не нужно. PlatformIO создаёт их сам при первой сборке и отладке, внутри абсолютные пути этой машины (`firmware.elf`, тулчейн, список `-I`). Они уже в `.gitignore` ESP32 и в `gitignore.snippet`. Подсветку даёт `compile_commands.json`, а не `c_cpp_properties.json`.

В `platformio.ini` вставьте `platformio.snippet.ini`. `lib_extra_dirs` указывает на `libs` в корне репозитория, `lib_deps` перечисляет библиотеки по полю `name` из их `library.json`. После сборки их исходники попадают в `compile_commands.json`, и Clangd открывает заголовки из `libs`. В `.gitignore` проекта добавьте строки из `gitignore.snippet`, если их ещё нет.

## Если базу нужно пересобрать

В `ESP32` файл обновляется сам при обычной сборке и при `pio run -t compiledb`. Скрипт `tools/pio_compiledb.py` подставляет компилятор из `PATH` этой машины (на Windows с `.exe`, на macOS без) и копирует базу в локальный пакет каждого фреймворка этого проекта (`framework-espidf`, `framework-arduino-avr` и остальные из `platformio.ini`). Оттуда Clangd находит её для заголовков, которые открываются из `~/.platformio`. На другой машине достаточно собрать проект: пути перепишутся на её `~/.platformio`.

Для остальных проектов та же строка в `platformio.ini`:

```ini
extra_scripts = pre:../tools/pio_compiledb.py
```

Путь `../` поправьте по глубине проекта относительно корня репозитория.

Если команда `pio` не находится, полный путь такой:

```bat
%USERPROFILE%\.platformio\penv\Scripts\pio.exe run -t compiledb
```

## Ограничения

Имя компилятора в базе — то, что реально лежит в `~/.platformio` на машине, где её собрали. После клона на macOS или на другом пользователе Windows запустите сборку проекта ещё раз.
