#!/usr/bin/env bash
# bus + N окон Node без заранее заданного id (назначение вручную в каждом).
#
#   ./host/run_mesh.sh           # bus + 2 node
#   ./host/run_mesh.sh 3         # bus + 3 node
#
set -euo pipefail

HOST_DIR="$(cd "$(dirname "$0")" && pwd)"
PORT="${SMCP_HOST_PORT:-19740}"
BUS="${HOST_DIR}/build/smcp_bus"
NODE="${HOST_DIR}/build/smcp_node"
COUNT="${1:-2}"

make -C "${HOST_DIR}" -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "Start manually:" >&2
  echo "  ${BUS} --port ${PORT}" >&2
  echo "  ${NODE} --port ${PORT}   # then: id 0x01" >&2
  exit 1
fi

osascript -e "tell application \"Terminal\" to activate" \
  -e "tell application \"Terminal\" to do script \"cd '${HOST_DIR}' && '${BUS}' --port ${PORT}; echo; read -p '[bus] Enter… '\""

sleep 0.4
for ((i = 1; i <= COUNT; i++)); do
  osascript -e "tell application \"Terminal\" to do script \"cd '${HOST_DIR}' && '${NODE}' --port ${PORT}; echo; read -p '[node] Enter… '\""
  sleep 0.2
done

echo "Opened bus + ${COUNT} node window(s)."
echo "In each node window:  id 0x01   /   id 0x10"
echo "Then in master:       open 0x10"
