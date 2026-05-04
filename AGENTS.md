## Cursor Cloud specific instructions

This is an embedded systems monorepo for a **winch controller** with three PlatformIO projects. Comments/docs are in Russian.

### Projects

| Directory | Target | Framework | Build status |
|---|---|---|---|
| `STM/` | STM32F407VGT6 | stm32cube (bare-metal HAL) | Builds cleanly |
| `AVR/` | ATmega2560 | Arduino | Builds cleanly |
| `STM ARDUINO/` | STM32 Black F407VG | Arduino | Pre-existing code error (`main.cpp` includes `.s` assembly file) |

### Build commands

```
cd STM && pio run                  # STM32 bare-metal firmware
cd AVR && pio run                  # AVR Arduino firmware
cd "STM ARDUINO" && pio run        # STM32 Arduino (currently broken in repo)
```

### Key caveats

- **No automated tests**: test directories contain only placeholder READMEs. Validate changes via `pio run` (successful compilation).
- **No lint tool configured**: use `pio check` for static analysis if needed, but it is not pre-configured.
- **AVR `lib_extra_dirs`**: `platformio.ini` references `~/Documents/Arduino/libraries` which doesn't exist in cloud. The required libraries (`EasyNextionLibrary`, `SdFat`) are installed as PlatformIO project-level dependencies instead via `pio pkg install`.
- **`STM ARDUINO/` is broken upstream**: `src/main.cpp` line 3 includes `startup_stm32f407xx.s` (an ARM assembly file) directly into C++ source. This is a pre-existing repo issue.
- **clangd**: run `pio run -t compiledb` in `STM/` to regenerate `compile_commands.json` (gitignored) after any build config changes.
- **No runtime testing possible**: firmware targets physical hardware (STM32/ATmega2560 boards, Nextion HMI display, SD cards). Successful cross-compilation is the primary validation.
