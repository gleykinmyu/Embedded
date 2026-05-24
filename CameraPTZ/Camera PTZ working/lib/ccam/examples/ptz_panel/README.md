# ptz_panel

Демо-прошивка: `app::PtzOperator` + `He130Camera` / `He130Pt`.

## Файлы

| Файл | Роль |
|------|------|
| `src/main.cpp` | точка входа, заглушки HAL / joystick / panel |
| `src/app/ptz_operator.*` | связка UI → ccam |
| `src/app/joystick.hpp` | интерфейс джойстика |
| `src/app/touch_panel.hpp` | интерфейс панели |

Код в `src/app/` относится к приложению, не к библиотеке `ccam`.
