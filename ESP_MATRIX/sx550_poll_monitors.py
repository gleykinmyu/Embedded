#!/usr/bin/env python3
"""Опрос камер на мониторах WJ-SX550: MS → PC/PN для каждого монитора."""

from __future__ import annotations

import argparse
import re
import time
from dataclasses import dataclass

import serial

STX, ETX = b"\x02", b"\x03"


@dataclass
class MonitorStatus:
    monitor: int
    camera: int | None
    camera_busy: bool | None
    monitor_busy: bool | None
    raw_pc: str
    raw_pn: str
    raw_ms: str


def ascii_rx(rx: bytes) -> str:
    return "".join(chr(b) if 32 <= b < 127 else "." for b in rx)


def transact(ser: serial.Serial, body: str, wait_s: float = 0.5) -> bytes:
    ser.reset_input_buffer()
    ser.write(STX + body.encode("ascii") + ETX)
    ser.flush()
    t0 = time.time()
    buf = bytearray()
    while time.time() - t0 < wait_s:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            buf.extend(chunk)
            time.sleep(0.015)
        else:
            time.sleep(0.008)
    return bytes(buf)


def open_matrix(port: str, baud: int) -> serial.Serial:
    ser = serial.Serial(
        port,
        baud,
        bytesize=8,
        parity=serial.PARITY_ODD,
        stopbits=serial.STOPBITS_ONE,
        rtscts=False,
        timeout=0.15,
        write_timeout=2.0,
    )
    ser.dtr = True
    ser.rts = True
    time.sleep(0.05)
    return ser


def login(ser: serial.Serial, user: str = "1", password: str = "12345") -> bool:
    rx = transact(ser, f"CMD:W{user}-{password}", wait_s=0.8)
    return b"WOK" in rx


def logout(ser: serial.Serial) -> None:
    transact(ser, "CMD:HLG", wait_s=0.4)


def parse_pc(rx: bytes) -> tuple[int | None, bool | None]:
    m = re.search(r"HC(\d{3})([RB])", ascii_rx(rx))
    if not m:
        return None, None
    return int(m.group(1)), m.group(2) == "B"


def parse_pn(rx: bytes) -> tuple[int | None, bool | None]:
    m = re.search(r"HN(\d{3})([RB])", ascii_rx(rx))
    if not m:
        return None, None
    return int(m.group(1)), m.group(2) == "B"


def poll_all_monitors(
    ser: serial.Serial,
    monitors: range | list[int] = range(1, 17),
    settle_s: float = 0.12,
    retries: int = 1,
) -> list[MonitorStatus]:
    """
    Для каждого монитора: MS → (settle) → PN/PC.

    Если PN.monitor == запрошенный — PC.camera это камера на этом мониторе
    (в т.ч. ранее выбранная «чужим» оператором, если удалось забрать MS).

    Если PN.monitor != запрошенный — монитор не взяли (priority/partition/busy),
    камеру чужого монитора так узнать нельзя.
    """
    rows: list[MonitorStatus] = []
    for mon in monitors:
        rx_ms = b""
        rx_pc = b""
        rx_pn = b""
        cam = None
        cam_busy = None
        mon_busy = None
        for _ in range(retries + 1):
            rx_ms = transact(ser, f"CMD:H{mon:02d}MS", wait_s=0.35)
            time.sleep(settle_s)
            rx_pn = transact(ser, "CMD:PN", wait_s=0.4)
            rx_pc = transact(ser, "CMD:PC", wait_s=0.4)
            got_mon, mon_busy = parse_pn(rx_pn)
            cam, cam_busy = parse_pc(rx_pc)
            if got_mon == mon:
                break
        rows.append(
            MonitorStatus(
                monitor=mon,
                camera=cam if parse_pn(rx_pn)[0] == mon else None,
                camera_busy=cam_busy,
                monitor_busy=mon_busy,
                raw_pc=ascii_rx(rx_pc),
                raw_pn=ascii_rx(rx_pn),
                raw_ms=ascii_rx(rx_ms),
            )
        )
    return rows


def main() -> int:
    ap = argparse.ArgumentParser(description="Poll SX550 monitor→camera map")
    ap.add_argument("--port", default="COM14")
    ap.add_argument("--baud", type=int, default=19200)
    ap.add_argument("--fallback-baud", type=int, default=9600)
    ap.add_argument("--user", default="1")
    ap.add_argument("--password", default="12345")
    ap.add_argument("--from", dest="mon_from", type=int, default=1)
    ap.add_argument("--to", dest="mon_to", type=int, default=16)
    args = ap.parse_args()

    bauds = [args.baud]
    if args.fallback_baud and args.fallback_baud != args.baud:
        bauds.append(args.fallback_baud)

    ser = None
    used = None
    for baud in bauds:
        try:
            ser = open_matrix(args.port, baud)
        except serial.SerialException as e:
            print(f"open {args.port}@{baud} fail: {e}")
            continue
        print(f"try login @ {baud} ...")
        if login(ser, args.user, args.password):
            used = baud
            print(f"OK login @ {baud}")
            break
        print(f"login fail @ {baud}: {ascii_rx(transact(ser, 'CMD:W1-12345', 0.3))}")
        ser.close()
        ser = None

    if ser is None or used is None:
        print("Не удалось залогиниться. Подними baud PC в Communication Speed до 19200?")
        return 2

    mons = range(args.mon_from, args.mon_to + 1)
    print(f"=== poll monitors {args.mon_from}..{args.mon_to} @ {used} ===")
    rows = poll_all_monitors(ser, mons)
    print(f"{'MON':>4}  {'CAM':>4}  {'OK':>3}  {'BUSY':>4}  PC                  PN")
    for r in rows:
        got = re.search(r"HN(\d{3})", r.raw_pn)
        acquired = got is not None and int(got.group(1)) == r.monitor
        cam = f"{r.camera:03d}" if r.camera is not None else "---"
        busy = "Y" if (r.monitor_busy or r.camera_busy) else "n"
        ok = "YES" if acquired else "NO"
        print(f"{r.monitor:4d}  {cam:>4}  {ok:>3}  {busy:>4}  {r.raw_pc:<18}  {r.raw_pn}")

    rx = transact(ser, "QSS", wait_s=0.8)
    print("QSS:", ascii_rx(rx))
    logout(ser)
    ser.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
