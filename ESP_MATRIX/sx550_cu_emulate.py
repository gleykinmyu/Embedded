#!/usr/bin/env python3
"""Emulate WV-CU550 on controller RS-232 (TX one port, RX the other).

Verified frame from sniff_session.log (100/100 CU, 593/593 MX):

    <addr> ENQ STX <addr> <body> ETX <addr> EOT
    ACK = <addr> ACK

CU addr=b'01', matrix addr=b'10'.  Baud 9600 8N1.
Default: TX=COM13 (to matrix), RX=COM12 (from matrix).
"""
from __future__ import annotations

import argparse
import sys
import threading
import time
from datetime import datetime

import serial

STX, ETX, EOT, ENQ, ACK = 0x02, 0x03, 0x04, 0x05, 0x06
CU, MX = b"01", b"10"


def ts() -> str:
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]


def enq_frame(addr: bytes, body: bytes) -> bytes:
    """addr ENQ STX addr body ETX addr EOT — addr действительно 3 раза."""
    return addr + bytes([ENQ, STX]) + addr + body + bytes([ETX]) + addr + bytes([EOT])


def ack_frame(addr: bytes) -> bytes:
    return addr + bytes([ACK])


class CuLink:
    def __init__(self, tx_port: str, rx_port: str, baud: int = 9600):
        if tx_port == rx_port:
            raise ValueError("tx and rx must be different ports")
        kw = dict(
            baudrate=baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            rtscts=False,
            timeout=0.02,
        )
        self.tx = serial.Serial(tx_port, **kw)  # write ONLY
        self.rx = serial.Serial(rx_port, **kw)  # read ONLY
        for s in (self.tx, self.rx):
            s.dtr = True
            s.rts = True
        self._stop = threading.Event()
        self._lock = threading.Lock()
        self._answers: list[str] = []
        self._acks = 0
        self._buf = bytearray()
        self._thr = threading.Thread(target=self._reader, daemon=True)
        self._thr.start()
        print(f"TX-only={tx_port}  RX-only={rx_port}  {baud} 8N1", flush=True)

    def close(self) -> None:
        self._stop.set()
        self._thr.join(1.5)
        self.tx.close()
        self.rx.close()

    def _reader(self) -> None:
        while not self._stop.is_set():
            chunk = self.rx.read(self.rx.in_waiting or 1)
            if chunk:
                with self._lock:
                    self._buf.extend(chunk)
                    self._consume_locked()
            else:
                time.sleep(0.001)

    def _consume_locked(self) -> None:
        """Reassemble fragmented MX frames; ACK every ENQ on TX port."""
        buf = self._buf
        while True:
            if len(buf) < 2:
                return

            # ACK: 31 30 06
            if buf.startswith(MX + bytes([ACK])):
                self._acks += 1
                print(f"[{ts()}] MX ACK", flush=True)
                del buf[:3]
                continue

            # ENQ frame: 31 30 05 02 31 30 <body> 03 31 30 04
            if buf.startswith(MX + bytes([ENQ])):
                # need STX
                if len(buf) < 4:
                    return
                if buf[3] != STX:
                    del buf[0]
                    continue
                # body addr should be 10
                if len(buf) < 6:
                    return
                e = buf.find(bytes([ETX]), 4)
                if e < 0:
                    return
                # trailer addr EOT
                need = e + 1 + 3  # ETX + addr(2) + EOT
                if len(buf) < need:
                    return
                payload = bytes(buf[4:e])  # after STX, before ETX
                if payload.startswith(MX):
                    body = payload[2:]
                else:
                    body = payload
                text = body.decode("latin1", "replace")
                # ACK the ENQ (controller duty)
                self.tx.write(ack_frame(CU))
                self.tx.flush()
                print(f"[{ts()}] MX ANS {text!r}", flush=True)
                self._answers.append(text)
                del buf[:need]
                continue

            # resync: drop until we see '1' of "10"
            if buf[0] != 0x31:
                del buf[0]
                continue
            if len(buf) >= 2 and buf[1] != 0x30:
                del buf[0]
                continue
            return

    def send(self, body: str | bytes, wait: float = 0.55) -> list[str]:
        raw = body.encode("ascii") if isinstance(body, str) else body
        pkt = enq_frame(CU, raw)
        with self._lock:
            self._answers.clear()
            self._acks = 0
        print(f"[{ts()}] CU {raw!r}", flush=True)
        print(f"         TX {pkt.hex(' ')}", flush=True)
        self.tx.write(pkt)
        self.tx.flush()
        t0 = time.time()
        while time.time() - t0 < wait:
            time.sleep(0.01)
        with self._lock:
            return list(self._answers)

    def hold(self, body: bytes, sec: float, period: float = 0.35) -> None:
        end = time.time() + sec
        while time.time() < end:
            self.send(body, wait=0.1)
            time.sleep(period)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--tx", default="COM13")
    ap.add_argument("--rx", default="COM12")
    ap.add_argument(
        "--only",
        choices=["login", "route", "left", "right", "up", "down", "zoom", "menu", "all"],
        default="login",
    )
    args = ap.parse_args()

    link = CuLink(args.tx, args.rx)
    time.sleep(0.4)
    try:
        ans = link.send("W01-12345", wait=1.0)
        print(f"  login ANS={ans}", flush=True)
        if args.only == "login":
            return 0

        for cmd in ("H02MS", "H02CS01"):
            ans = link.send(cmd)
            print(f"  {cmd} ANS={ans}", flush=True)
            time.sleep(0.2)
        if args.only == "route":
            return 0

        stop = b"L01ACBD"
        seq = {
            "left": (b"L01ACFHB@@@", b"L01ACFHBB@@", b"L01ACFHBC@@"),
            "right": (b"L01ACFLB@@@", b"L01ACFLBB@@", b"L01ACFLBC@@"),
            "up": (b"L01ACFJB@@@", b"L01ACFJB@B@", b"L01ACFJB@C@"),
            "down": (b"L01ACFNB@@@", b"L01ACFNB@B@", b"L01ACFNB@C@"),
        }
        names = list(seq) if args.only == "all" else ([args.only] if args.only in seq else [])
        for name in names:
            a, b, c = seq[name]
            print(f"=== {name.upper()} ===", flush=True)
            link.send(a, 0.2)
            link.hold(b, 0.5)
            link.hold(c, 3.0)
            link.send(stop, 0.3)
            time.sleep(0.3)

        if args.only in ("zoom", "all"):
            print("=== ZOOM ===", flush=True)
            link.hold(b"L01ABBL", 2.0)
            link.send(b"L01ABBD", 0.3)
            link.hold(b"L01ABBH", 2.0)
            link.send(b"L01ABBD", 0.3)

        if args.only in ("menu", "all"):
            print("=== MENU ===", flush=True)
            print("  open ANS=", link.send(b"S01AID@", 0.7), flush=True)
            time.sleep(1.0)
            for step in (b"S01AIDD", b"S01AIDE", b"S01AIDA"):
                print(" ", step, "ANS=", link.send(step, 0.4), flush=True)
                time.sleep(0.4)

        print("done", flush=True)
    finally:
        link.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
