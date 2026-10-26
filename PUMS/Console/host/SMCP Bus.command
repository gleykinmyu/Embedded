#!/bin/bash
# macOS: двойной клик → окно шины (не закрывается само).
cd "$(dirname "$0")"
make -s bus 2>/dev/null || make bus
echo "=== SMCP Bus — оставь это окно открытым ==="
./build/smcp_bus --port "${SMCP_HOST_PORT:-19740}"
echo
echo "[bus] exit $? — Enter закрыть"
read -r _
