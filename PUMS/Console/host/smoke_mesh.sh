#!/usr/bin/env bash
set -euo pipefail

HOST_DIR="$(cd "$(dirname "$0")" && pwd)"
PORT="${SMCP_HOST_PORT:-19744}"
BUS="${HOST_DIR}/build/smcp_bus"
NODE="${HOST_DIR}/build/smcp_node"

make -C "${HOST_DIR}" -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

LOG_BUS="$(mktemp -t smcp_bus)"
LOG_S="$(mktemp -t smcp_ns)"
LOG_M="$(mktemp -t smcp_nm)"
FIFO_S="$(mktemp -u -t smcp_sfifo)"
FIFO_M="$(mktemp -u -t smcp_mfifo)"
mkfifo "${FIFO_S}" "${FIFO_M}"

"${BUS}" --port "${PORT}" >"${LOG_BUS}" 2>&1 &
BUS_PID=$!
cleanup() {
  kill "${BUS_PID}" "${SLAVE_PID:-}" "${MASTER_PID:-}" 2>/dev/null || true
  wait "${BUS_PID}" 2>/dev/null || true
  rm -f "${FIFO_S}" "${FIFO_M}"
}
trap cleanup EXIT
sleep 0.3

"${NODE}" --port "${PORT}" <"${FIFO_S}" >"${LOG_S}" 2>&1 &
SLAVE_PID=$!
exec 3>"${FIFO_S}"

"${NODE}" --port "${PORT}" <"${FIFO_M}" >"${LOG_M}" 2>&1 &
MASTER_PID=$!
exec 4>"${FIFO_M}"

sleep 0.8
printf 'id 0x10\n' >&3
printf 'id 0x01\n' >&4
sleep 0.8
printf 'open 0x10\n' >&4
sleep 1.5
printf 'set 0x10 0xA1 0123456789ABCDEF0123456789ABCDEF\n' >&4
sleep 2.5
printf 'get 0x10 0xA1\n' >&4
sleep 2.5
printf 'quit\n' >&4
printf 'quit\n' >&3
sleep 0.5

exec 3>&- 4>&-
wait "${MASTER_PID}" || true
wait "${SLAVE_PID}" || true

echo "--- master ---"
cat "${LOG_M}"
grep -q 'ok: session Open' "${LOG_M}" || { echo "FAIL: not Open"; exit 1; }
grep -q 'ok: len=16' "${LOG_M}" || { echo "FAIL: get"; exit 1; }
echo "smoke_mesh OK"
