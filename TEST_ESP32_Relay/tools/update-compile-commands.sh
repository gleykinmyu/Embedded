#!/bin/sh
# Обновляет compile_commands.json для clangd: pio run -t compiledb
# Берёт default_envs из platformio.ini (или все env, если не задано).
# Из корня проекта:  ./tools/update-compile-commands.sh
# Конкретный env:    ./tools/update-compile-commands.sh --env esp32s3_poe_eth_8di_8ro
# Без перезапуска clangd:  ./tools/update-compile-commands.sh --no-restart
set -e
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

NO_RESTART=0
ENV_NAME=
while [ $# -gt 0 ]; do
    case "$1" in
    --no-restart|-n) NO_RESTART=1 ;;
    --env|-e)
        shift
        ENV_NAME=${1:-}
        if [ -z "$ENV_NAME" ]; then
            echo "Missing value for --env" >&2
            exit 2
        fi
        ;;
    -h|--help)
        echo "Usage: $0 [--env NAME] [--no-restart]"
        exit 0
        ;;
    *)
        echo "Unknown argument: $1" >&2
        exit 2
        ;;
    esac
    shift
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

set -- run -t compiledb
if [ -n "$ENV_NAME" ]; then
    set -- "$@" -e "$ENV_NAME"
    echo "env: $ENV_NAME"
else
    echo "env: (default_envs / all)"
fi
"$PIO" "$@"

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
