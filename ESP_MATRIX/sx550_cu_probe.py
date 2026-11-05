#!/usr/bin/env python3
"""Probe which COM is controller-TX: send login on one, listen on the other."""
from __future__ import annotations

import sys
import threading
import time
from datetime import datetime

import serial

STX, ETX, EOT, ENQ, ACK = 0x02, 0x03, 0x04, 0x05, 0x06
CU, MX = b"01", b"10"


def ts() -> str:
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]


def open_port(name: str) -> serial.Serial:
    s = serial.Serial(
        name,
        9600,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_NONE,
        stopbits=serial.STOPBITS_ONE,
        rtscts=False,
        timeout=0.05,
    )
    s.dtr = True
    s.rts = True
    return s


def frame(body: bytes) -> bytes:
    return CU + bytes([ENQ, STX]) + CU + body + bytes([ETX]) + CU + bytes([EOT])


def listen(rx: serial.Serial, stop: threading.Event, label: str, got: list) -> None:
    buf = bytearray()
    while not stop.is_set():
        chunk = rx.read(rx.in_waiting or 1)
        if not chunk:
            continue
        buf.extend(chunk)
        hx = " ".join(f"{b:02X}" for b in chunk)
        asc = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
        print(f"[{ts()}] {label} RX {hx} |{asc}|", flush=True)
        # parse bodies
        i = 0
        while True:
            s = buf.find(bytes([STX]), i)
            if s < 0:
                break
            e = buf.find(bytes([ETX]), s + 1)
            if e < 0:
                break
            payload = bytes(buf[s + 1 : e])
            body = payload[2:] if len(payload) >= 2 else payload
            text = body.decode("latin1", "replace")
            got.append(text)
            print(f"[{ts()}] {label} BODY {text!r}", flush=True)
            i = e + 1
        if len(buf) > 256:
            del buf[:-64]


def probe(tx_name: str, rx_name: str) -> list[str]:
    print(f"\n=== PROBE TX={tx_name}  RX={rx_name} ===", flush=True)
    tx = open_port(tx_name)
    rx = open_port(rx_name)
    stop = threading.Event()
    got: list[str] = []
    thr = threading.Thread(target=listen, args=(rx, stop, rx_name, got), daemon=True)
    thr.start()
    time.sleep(0.8)  # hear background?
    print(f"[{ts()}] background so far: {got}", flush=True)

    body = b"W01-12345"
    print(f"[{ts()}] SEND login on {tx_name}", flush=True)
    tx.write(frame(body))
    tx.flush()
    # if we hear matrix ENQ on rx, ACK on tx
    t0 = time.time()
    while time.time() - t0 < 2.0:
        # opportunistic ACK if ENQ seen — crude: just ACK periodically after send
        time.sleep(0.05)
    # also try ACKing a few times in case matrix spoke
    for _ in range(3):
        tx.write(CU + bytes([ACK]))
        tx.flush()
        time.sleep(0.05)

    time.sleep(1.5)
    stop.set()
    thr.join(1)
    tx.close()
    rx.close()
    print(f"[{ts()}] result bodies: {got}", flush=True)
    return got


def main() -> int:
    # Try both directions — only one TX at a time
    a = probe("COM13", "COM12")
    time.sleep(0.5)
    b = probe("COM12", "COM13")

    def score(bodies: list[str]) -> int:
        hits = 0
        for x in bodies:
            if x in ("WOK", "HZ016") or x.startswith(("HN", "HC", "N0", "C0")):
                hits += 1
        return hits

    sa, sb = score(a), score(b)
    print("\n=== SUMMARY ===", flush=True)
    print(f"TX=COM13 RX=COM12 score={sa}  samples={a[:8]}", flush=True)
    print(f"TX=COM12 RX=COM13 score={sb}  samples={b[:8]}", flush=True)
    if sa == 0 and sb == 0:
        print("NO MATRIX TRAFFIC — check wiring / which pair is live", flush=True)
    elif sb > sa:
        print("USE: TX=COM12  RX=COM13", flush=True)
    else:
        print("USE: TX=COM13  RX=COM12", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
