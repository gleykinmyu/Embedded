#!/bin/sh
# Обновляет compile_commands.json для clangd: pio run -t compiledb -e tester
# Из корня проекта:  ./tools/update-compile-commands.sh
# Без перезапуска clangd:  ./tools/update-compile-commands.sh --no-restart
set -e
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

NO_RESTART=0
for arg in "$@"; do
    case "$arg" in
    --no-restart|-n) NO_RESTART=1 ;;
    -h|--help)
        echo "Usage: $0 [--no-restart]"
        exit 0
        ;;
    *)
        echo "Unknown argument: $arg" >&2
        exit 2
        ;;
    esac
done

find_pio() {
    if command -v pio >/dev/null 2>&1; then
        command -v pio
        return
    fi
    if command -v platformio >/dev/null 2>&1; then
        command -v platformio
        return
    fi
    for c in \
        "$HOME/.platformio/penv/bin/pio" \
        "$HOME/.platformio/penv/bin/platformio"
    do
        if [ -x "$c" ]; then
            echo "$c"
            return
        fi
    done
    echo "PlatformIO CLI не найден (pio / platformio)." >&2
    echo "Установите PlatformIO или добавьте pio в PATH." >&2
    echo "Ожидаемый путь: ~/.platformio/penv/bin/pio" >&2
    exit 1
}

PIO=$(find_pio)
echo "pio: $PIO"
echo "cwd: $ROOT"
"$PIO" run -t compiledb -e tester

DB="$ROOT/compile_commands.json"
if [ ! -f "$DB" ]; then
    echo "compile_commands.json не появился в $ROOT" >&2
    exit 1
fi

if [ "$NO_RESTART" -eq 1 ]; then
    echo "Готово: $DB"
    echo "Перезапустите clangd: Command Palette -> Clangd: Restart language server"
    exit 0
fi

if command -v pkill >/dev/null 2>&1 && pkill -x clangd 2>/dev/null; then
    echo "Готово: $DB  (clangd перезапущен)"
else
    echo "Готово: $DB"
    echo "Перезапустите clangd: Command Palette -> Clangd: Restart language server"
fi
