#!/bin/sh
# Cursor/VS Code: путь в "clangd.path". GUI часто даёт урезанный PATH — дополняем типичные каталоги.
# Порядок: portable (./tools/bootstrap-clangd.sh) → Homebrew llvm → PATH → /usr/bin/clangd.
#
# Установка portable: из корня репозитория — ./tools/bootstrap-clangd.sh
# Иначе: brew install llvm или xcode-select --install

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
PORTABLE_DIR="$ROOT/tools/clangd-portable"
if [ -d "$PORTABLE_DIR" ]; then
    for p in "$PORTABLE_DIR"/clangd_*/bin/clangd; do
        if [ -x "$p" ] && "$p" --version >/dev/null 2>&1; then
            exec "$p" "$@"
        fi
    done
fi

# Типичные бинарники Homebrew (в т.ч. keg-only llvm без link в /opt/homebrew/bin)
PATH="/opt/homebrew/opt/llvm/bin:/usr/local/opt/llvm/bin:/opt/homebrew/bin:/usr/local/bin:$PATH"
export PATH

if [ -n "$HOMEBREW_PREFIX" ] && [ -x "$HOMEBREW_PREFIX/opt/llvm/bin/clangd" ]; then
    exec "$HOMEBREW_PREFIX/opt/llvm/bin/clangd" "$@"
fi
if [ -x /opt/homebrew/opt/llvm/bin/clangd ]; then
    exec /opt/homebrew/opt/llvm/bin/clangd "$@"
fi
if [ -x /usr/local/opt/llvm/bin/clangd ]; then
    exec /usr/local/opt/llvm/bin/clangd "$@"
fi

# Версии llvm@17, llvm@18, … (Homebrew)
for base in /opt/homebrew/opt /usr/local/opt; do
    for llvm in "$base"/llvm@*/bin/clangd; do
        if [ -x "$llvm" ]; then
            exec "$llvm" "$@"
        fi
    done
done

CLANGD_PATH=$(command -v clangd 2>/dev/null)
if [ -n "$CLANGD_PATH" ] && [ -x "$CLANGD_PATH" ] && "$CLANGD_PATH" --version >/dev/null 2>&1; then
    exec "$CLANGD_PATH" "$@"
fi

if [ -x /usr/bin/clangd ] && /usr/bin/clangd --version >/dev/null 2>&1; then
    exec /usr/bin/clangd "$@"
fi

echo "clangd-select: не найден рабочий clangd." >&2
echo "  Из корня репозитория: ./tools/bootstrap-clangd.sh" >&2
echo "  Или: brew install llvm / xcode-select --install" >&2
exit 127
