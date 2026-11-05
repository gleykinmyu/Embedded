#!/usr/bin/env python3
"""Dual-port RS-232 sniffer: log COM12 + COM13 (9600 8N1) + external MARK lines."""
from __future__ import annotations

import argparse
import sys
import threading
import time
from datetime import datetime
from pathlib import Path

import serial

_log_lock = threading.Lock()
_log_fp = None


def ts() -> str:
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]


def log(msg: str) -> None:
    line = f"[{ts()}] {msg}"
    with _log_lock:
        print(line, flush=True)
        if _log_fp is not None:
            _log_fp.write(line + "\n")
            _log_fp.flush()


def hexdump(data: bytes) -> str:
    hx = " ".join(f"{b:02X}" for b in data)
    asc = "".join(chr(b) if 32 <= b < 127 else "." for b in data)
    return f"{hx}  |{asc}|"


def reader(port: str, baud: int, label: str, stop: threading.Event) -> None:
    try:
        ser = serial.Serial(
            port,
            baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            rtscts=False,
            timeout=0.05,
        )
        ser.dtr = True
        ser.rts = True
    except Exception as e:
        log(f"{label} OPEN FAIL: {e}")
        return

    log(f"{label} OPEN {port} {baud} 8N1")
    buf = bytearray()
    last = time.time()
    try:
        while not stop.is_set():
            n = ser.in_waiting
            chunk = ser.read(n or 1)
            now = time.time()
            if chunk:
                if buf and (now - last) > 0.03:
                    log(f"{label} {hexdump(bytes(buf))}")
                    buf.clear()
                buf.extend(chunk)
                last = now
            elif buf and (now - last) > 0.03:
                log(f"{label} {hexdump(bytes(buf))}")
                buf.clear()
            else:
                time.sleep(0.001)
    except Exception as e:
        log(f"{label} ERR: {e}")
    finally:
        if buf:
            log(f"{label} {hexdump(bytes(buf))}")
        ser.close()
        log(f"{label} CLOSED")


def mark_watcher(mark_path: Path, stop: threading.Event) -> None:
    """Poll mark file: each new line becomes ### MARK: ... in the log."""
    mark_path.write_text("", encoding="utf-8")
    pos = 0
    while not stop.is_set():
        try:
            data = mark_path.read_text(encoding="utf-8")
            if len(data) > pos:
                chunk = data[pos:]
                pos = len(data)
                for raw in chunk.splitlines():
                    text = raw.strip()
                    if text:
                        log(f"### MARK: {text}")
        except OSError:
            pass
        time.sleep(0.05)


def main() -> int:
    global _log_fp
    ap = argparse.ArgumentParser()
    ap.add_argument("--a", default="COM12")
    ap.add_argument("--b", default="COM13")
    ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument(
        "--log",
        default="sniff_session.log",
        help="tee log file path",
    )
    ap.add_argument(
        "--mark",
        default="sniff_mark.txt",
        help="append a line here to stamp MARK into the log",
    )
    args = ap.parse_args()

    log_path = Path(args.log)
    mark_path = Path(args.mark)
    _log_fp = open(log_path, "a", encoding="utf-8")

    stop = threading.Event()
    threads = [
        threading.Thread(
            target=reader, args=(args.a, args.baud, "A/" + args.a, stop), daemon=True
        ),
        threading.Thread(
            target=reader, args=(args.b, args.baud, "B/" + args.b, stop), daemon=True
        ),
        threading.Thread(
            target=mark_watcher, args=(mark_path, stop), daemon=True
        ),
    ]
    for t in threads:
        t.start()

    log(f"READY — {args.a}+{args.b} @ {args.baud} 8N1")
    log(f"LOG={log_path.resolve()} MARK={mark_path.resolve()}")
    try:
        while any(t.is_alive() for t in threads[:2]):
            time.sleep(0.2)
    except KeyboardInterrupt:
        log("stopping...")
        stop.set()
        for t in threads:
            t.join(2)
    finally:
        if _log_fp is not None:
            _log_fp.close()
            _log_fp = None
    return 0


if __name__ == "__main__":
    sys.exit(main())
