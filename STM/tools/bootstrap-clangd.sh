#!/bin/sh
# Скачивает официальный clangd (github.com/clangd/clangd) в tools/clangd-portable/ — без Homebrew и Xcode.
# macOS: clangd-mac-VERSION.zip. Linux x86_64: clangd-linux-VERSION.zip.
set -e
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
VERSION=22.1.0
DEST="$ROOT/tools/clangd-portable"
OS=$(uname -s)
ARCH=$(uname -m)

case "$OS" in
Darwin)
    ZIP_NAME="clangd-mac-${VERSION}.zip"
    ;;
Linux)
    if [ "$ARCH" = aarch64 ] || [ "$ARCH" = arm64 ]; then
        echo "Для Linux ARM скачайте архив вручную: https://github.com/clangd/clangd/releases/tag/${VERSION}" >&2
        exit 1
    fi
    ZIP_NAME="clangd-linux-${VERSION}.zip"
    ;;
*)
    echo "ОС не поддержана скриптом: $OS. Релизы: https://github.com/clangd/clangd/releases" >&2
    exit 1
    ;;
esac

URL="https://github.com/clangd/clangd/releases/download/${VERSION}/${ZIP_NAME}"
BIN="$DEST/clangd_${VERSION}/bin/clangd"

if [ -x "$BIN" ] && "$BIN" --version >/dev/null 2>&1; then
    echo "Уже установлено: $BIN"
    "$BIN" --version
    exit 0
fi

echo "Скачивание $URL ..."
mkdir -p "$DEST"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
curl -fsSL -o "$TMP/$ZIP_NAME" "$URL"
rm -rf "$DEST/clangd_${VERSION}"
(cd "$DEST" && unzip -q "$TMP/$ZIP_NAME")
chmod +x "$BIN"
echo "Готово: $BIN"
"$BIN" --version
