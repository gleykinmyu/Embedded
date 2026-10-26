#!/bin/bash
# macOS: двойной клик → узел. Id назначаешь вручную: id 0x01
cd "$(dirname "$0")"
make -s node 2>/dev/null || make node
echo "=== SMCP Node — сначала: id 0x01  (или другой) ==="
./build/smcp_node --port "${SMCP_HOST_PORT:-19740}"
echo
echo "[node] exit $? — Enter закрыть"
read -r _
