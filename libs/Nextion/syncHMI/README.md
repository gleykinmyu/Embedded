# syncHMI — синхронизация Nextion `.HMI` с C++

Инструменты для разбора бинарного проекта Nextion Editor и генерации C++-конфига: panel id компонентов, objname, привязка к `nex::comp::*`.

## Файлы в папке

| Файл | Назначение |
|------|------------|
| `hmi2config.py` | Генератор `nexHmiConfig.hpp` из `.HMI` |
| `nexHmiSync.hpp` | X-макросы, lookup `name()`, макросы для полей виджетов (`HMI_COMP_*`) |
| `Nextion2Text.py` | Парсер бинарного `.HMI` (Max Zuidberg, MPL-2.0); используется `hmi2config.py` |

Сгенерированный артефакт проекта обычно лежит в **`src/nexHmiConfig.hpp`** (не в этой папке).

`__pycache__/` — локальный кэш Python, в git не коммитится.

## Требования

- Python 3.8+
- Файл проекта Nextion Editor: `*.HMI` / `*.hmi`
- Сборка: `-I../..` (Nextion root, см. `examples/stm/platformio.ini`)

## Быстрый старт

Из каталога примера `examples/stm/`:

```powershell
python ../../syncHMI/hmi2config.py
```

По умолчанию: вход — единственный `*.HMI` в `src/`, выход — `src/nexHmiConfig.hpp`.

Явные пути:

```powershell
python ../../syncHMI/hmi2config.py `
  -i "src/ПУМС 2.2.HMI" `
  -o src/nexHmiConfig.hpp
```

После правок в Nextion Editor (обновить только `GENERATED-HMI-*` блоки, ручной код сохраняется):

```powershell
python lib/Nextion/syncHMI/hmi2config.py --update `
  -i "src/ПУМС 2.2.HMI" `
  -o src/nexHmiConfig.hpp
```

## Аргументы `hmi2config.py`

| Флаг | Описание |
|------|----------|
| `-i`, `--input` | Путь к `.HMI` |
| `-o`, `--output` | Путь к `.hpp` (default: `src/nexHmiConfig.hpp`) |
| `--src-dir` | Каталог поиска `.HMI` (default: `src`) |
| `-n`, `--namespace` | C++ namespace (default: `nex::hmi`) |
| `--update` | Патч существующего файла по маркерам `GENERATED-HMI-BEGIN/END` |
| `--stdout` | Вывод в stdout без записи файла |

## Что генерируется

`nexHmiConfig.hpp` содержит:

1. **`summary`** — модель панели, число страниц.
2. **`pages`** — для каждой страницы:
   - `#define HMI_PAGE_<objname>(X)` — единый список компонентов;
   - `struct Page_<objname>` с `kPageId`, `enum Id : uint8_t`, `kNames[]`.

Пример:

```cpp
#define HMI_PAGE_pgWork(X) \
    X(pgWork, 0) \
    X(tc, 1) \
    X(b0, 2) \
    ...

struct Page_pgWork {
    static constexpr uint8_t kPageId = 5u;

    enum Id : uint8_t {
        HMI_PAGE_pgWork(HMI_ENUM_ITEM)   // pgWork = 0, tc = 1, b0 = 2, ...
    };

    static constexpr const char* kNames[] = {
        HMI_PAGE_pgWork(HMI_NAME_ITEM)   // "pgWork", "tc", "b0", ...
    };
};
```

X-макрос `X(sym, id)` задаётся один раз: **sym** — и имя в enum, и NIS objname (через `HMI_STRINGIFY(sym)`). Третий аргумент со строкой в кавычках не нужен, если objname в HMI совпадает с валидным C-идентификатором.

Ручной код добавляйте **вне** блоков:

```cpp
// GENERATED-HMI-BEGIN:summary
...
// GENERATED-HMI-END:summary
```

## Использование в C++

Подключение:

```cpp
#include "nexHmiConfig.hpp"   // включает syncHMI/nexHmiSync.hpp
```

### Lookup objname по panel id

```cpp
using namespace nex::hmi;

// touch: route.comp
const char* obj = name<Page_pgWork>(e.route.comp);

// через enum
const char* obj2 = name<Page_pgWork>(Page_pgWork::b0);
```

### Виджеты на `nex::Page<>`

Макросы из `nexHmiSync.hpp` связывают поле с конструктором `comp::* (IPage& owner, const Literal& name, uint8_t id)`:

```cpp
struct WorkPage : nex::Page<48> {
    HMI_PAGE_CFG(pgWork)   // using PageCfg = nex::hmi::Page_pgWork;

    comp::Button<> b0;
    comp::Button<> tc;

    WorkPage(nex::IAppUI& app) noexcept
        : Page<48>(app, HMI_COMP_OBJNAME(pgWork), PageCfg::kPageId)
        , HMI_COMP_INIT(tc)
        , HMI_COMP_INIT(b0)
    {}
};
```

Краткие варианты:

```cpp
HMI_PAGE_CFG(pgWork)
HMI_WIDGET(comp::Button<>, b0);           // поле + default member init

HMI_COMP_ID(Page_pgWork, b0)              // uint8_t 2
HMI_COMP_OBJNAME(b0)                      // "b0"
HMI_COMP_ARGS(Page_pgWork, b0)            // *this, "b0", 2u
```

Справка по макросам — комментарии в `nexHmiSync.hpp`.

## Типичный workflow

```
Nextion Editor → сохранить .HMI в src/
        ↓
python lib/Nextion/syncHMI/hmi2config.py --update
        ↓
git diff src/nexHmiConfig.hpp
        ↓
pio run
```

## Ограничения

- **`kPageId`** страницы — порядок страниц в проекте HMI (0, 1, 2, …), как в `page N` в `Program.s`. Внутри `.pa` у объекта Page `id` всегда 0.
- **objname** должен быть валидным C-идентификатором (`b0`, `ctrlClear`). Иначе генератор санитизирует имя или подставляет строку в кавычках — NIS-имя на панели может не совпасть с `#sym`.
- При «дырах» в нумерации id на странице массив `kNames[]` может содержать `nullptr` на пропущенных индексах (редкий случай).

## Nextion2Text.py (опционально)

Отдельная утилита — человекочитаемый текст/JSON из `.HMI` (атрибуты, event-код). `hmi2config.py` использует только её парсер (`HMI`, `Page`, `Component`).

```powershell
python ../../syncHMI/Nextion2Text.py -i project.HMI -o out_txt/
```

См. `--help` у обоих скриптов.
