#!/usr/bin/env python3
"""
Многоканальный sniffer RS422/RS485 через USB-Serial (Waveshare и др.).

Слушает COM-порты, помечает чей TX на линии (controller / device),
пишет сырые чанки (пауза на линии → граница записи). Без привязки к протоколу.

Конфиг: config.json рядом со скриптом.
Документация: README.md в этой же папке.

Запуск:
  python tools/rs422_sniff/sniff.py
  python tools/rs422_sniff/sniff.py --list-ports
  python tools/rs422_sniff/sniff.py --self-test
"""

from __future__ import annotations

import argparse
import json
import sys
import threading
import time
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Any, Literal

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print(
        "Need pyserial:  pip install -r tools/rs422_sniff/requirements.txt",
        file=sys.stderr,
    )
    sys.exit(1)

ROOT = Path(__file__).resolve().parent
DEFAULT_CONFIG = ROOT / "config.json"

MAX_CHUNK = 4096

TX_CONTROLLER = "controller"
TX_DEVICE = "device"
TX_ALIASES = {
    "controller": TX_CONTROLLER,
    "ctrl": TX_CONTROLLER,
    "master": TX_CONTROLLER,
    "host": TX_CONTROLLER,
    "device": TX_DEVICE,
    "dev": TX_DEVICE,
    "slave": TX_DEVICE,
    "actuator": TX_DEVICE,
    "unit": TX_DEVICE,
}
TX_TAG = {
    TX_CONTROLLER: "CTRL",
    TX_DEVICE: "DEV",
}

PRINT_LOCK = threading.Lock()

TxFrom = Literal["controller", "device"]


@dataclass
class ChannelCfg:
    name: str
    port: str
    tx_from: TxFrom
    enabled: bool = True
    baud: int | None = None
    comment: str = ""


@dataclass
class AppCfg:
    baud: int = 9600
    bytesize: int = 8
    parity: str = "N"
    stopbits: float = 1
    flush_timeout_ms: int = 80
    show_hex: bool = True
    show_ascii: bool = True
    log_dir: str = "logs"
    channels: list[ChannelCfg] = field(default_factory=list)


class ConfigError(ValueError):
    pass


def normalize_tx_from(value: Any, index: int) -> TxFrom:
    if value is None or str(value).strip() == "":
        raise ConfigError(
            f"channels[{index}]: missing 'tx_from' "
            f"('{TX_CONTROLLER}' = TX контроллера, '{TX_DEVICE}' = TX исполнителя)"
        )
    key = str(value).strip().lower()
    if key not in TX_ALIASES:
        allowed = ", ".join(sorted(set(TX_ALIASES.values())))
        raise ConfigError(
            f"channels[{index}]: unknown tx_from={value!r}; use {allowed} "
            f"(aliases: ctrl/master/host, dev/slave/actuator)"
        )
    return TX_ALIASES[key]  # type: ignore[return-value]


def load_config(path: Path) -> AppCfg:
    try:
        raw: dict[str, Any] = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        raise ConfigError(f"Invalid JSON in {path}: {e}") from e

    if not isinstance(raw, dict):
        raise ConfigError(f"Config root must be an object: {path}")

    # Устаревшее поле framing игнорируем (всегда raw).
    if "framing" in raw:
        print(
            "Note: 'framing' is ignored — sniffer always logs raw chunks",
            file=sys.stderr,
        )

    channels: list[ChannelCfg] = []
    for i, c in enumerate(raw.get("channels", [])):
        if not isinstance(c, dict):
            raise ConfigError(f"channels[{i}] must be an object")
        if "port" not in c or not str(c["port"]).strip():
            raise ConfigError(f"channels[{i}]: missing non-empty 'port'")
        baud = c.get("baud")
        if baud is not None:
            baud = int(baud)
            if baud <= 0:
                raise ConfigError(f"channels[{i}]: baud must be > 0")
        channels.append(
            ChannelCfg(
                name=str(c.get("name", c["port"])),
                port=str(c["port"]).strip(),
                tx_from=normalize_tx_from(c.get("tx_from"), i),
                enabled=bool(c.get("enabled", True)),
                baud=baud,
                comment=str(c.get("comment", "")),
            )
        )

    baud = int(raw.get("baud", 9600))
    if baud <= 0:
        raise ConfigError("baud must be > 0")

    flush_ms = int(raw.get("flush_timeout_ms", 80))
    if flush_ms < 0:
        raise ConfigError("flush_timeout_ms must be >= 0")

    return AppCfg(
        baud=baud,
        bytesize=int(raw.get("bytesize", 8)),
        parity=str(raw.get("parity", "N")).upper()[:1],
        stopbits=float(raw.get("stopbits", 1)),
        flush_timeout_ms=flush_ms,
        show_hex=bool(raw.get("show_hex", True)),
        show_ascii=bool(raw.get("show_ascii", True)),
        log_dir=str(raw.get("log_dir", "logs")),
        channels=channels,
    )


def parity_const(p: str) -> str:
    return {
        "N": serial.PARITY_NONE,
        "E": serial.PARITY_EVEN,
        "O": serial.PARITY_ODD,
        "M": serial.PARITY_MARK,
        "S": serial.PARITY_SPACE,
    }.get(p, serial.PARITY_NONE)


def bytesize_const(n: int) -> int:
    return {
        5: serial.FIVEBITS,
        6: serial.SIXBITS,
        7: serial.SEVENBITS,
        8: serial.EIGHTBITS,
    }.get(n, serial.EIGHTBITS)


def stopbits_const(n: float) -> float:
    if n == 1.5:
        return serial.STOPBITS_ONE_POINT_FIVE
    if n >= 2:
        return serial.STOPBITS_TWO
    return serial.STOPBITS_ONE


def fmt_line_settings(cfg: AppCfg) -> str:
    sb = cfg.stopbits
    sb_s = "1.5" if sb == 1.5 else str(int(sb)) if sb == int(sb) else str(sb)
    return f"{cfg.baud} {cfg.bytesize}{cfg.parity}{sb_s}"


def fmt_ts() -> str:
    now = datetime.now()
    return now.strftime("%H:%M:%S.") + f"{now.microsecond // 1000:03d}"


def ascii_preview(data: bytes) -> str:
    """Подсказка: только печатный ASCII; всё остальное — \\xHH. Истина — hex."""
    parts: list[str] = []
    for b in data:
        if 32 <= b < 127:
            parts.append(chr(b))
        else:
            parts.append(f"\\x{b:02X}")
    return "".join(parts)


class RawChunker:
    """Сырые чанки: пауза на линии или MAX_CHUNK → одна запись в лог."""

    def __init__(self, flush_timeout_s: float) -> None:
        self.flush_timeout_s = flush_timeout_s
        self.buf = bytearray()
        self.last_rx = 0.0

    def feed(self, chunk: bytes) -> list[bytes]:
        if not chunk:
            return []
        self.last_rx = time.monotonic()
        self.buf.extend(chunk)
        out: list[bytes] = []
        while len(self.buf) >= MAX_CHUNK:
            out.append(bytes(self.buf[:MAX_CHUNK]))
            del self.buf[:MAX_CHUNK]
        return out

    def poll_flush(self) -> list[bytes]:
        if not self.buf:
            return []
        if (time.monotonic() - self.last_rx) < self.flush_timeout_s:
            return []
        frame = bytes(self.buf)
        self.buf.clear()
        return [frame]


class Sniffer:
    def __init__(self, cfg: AppCfg, config_path: Path) -> None:
        self.cfg = cfg
        self.config_path = config_path
        self.stop = threading.Event()
        self.log_fp = None
        self.threads: list[threading.Thread] = []

    def start(self) -> None:
        active = [c for c in self.cfg.channels if c.enabled]
        if not active:
            raise SystemExit("No enabled channels in config")

        log_dir = Path(self.cfg.log_dir)
        if not log_dir.is_absolute():
            log_dir = self.config_path.parent / log_dir
        log_dir.mkdir(parents=True, exist_ok=True)
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        log_path = log_dir / f"rs422_{stamp}.log"
        self.log_fp = log_path.open("w", encoding="utf-8", newline="\n")

        print(f"Config: {self.config_path}")
        print(f"Log:    {log_path}")
        print(f"Line:   {fmt_line_settings(self.cfg)}  (raw chunks, gap={self.cfg.flush_timeout_ms} ms)")
        print("Channels (port listens to TX of …):")
        for ch in active:
            baud = ch.baud if ch.baud is not None else self.cfg.baud
            tag = TX_TAG[ch.tx_from]
            note = f"  ({ch.comment})" if ch.comment else ""
            print(f"  [{ch.name}] {ch.port} @ {baud}  tx_from={ch.tx_from} ({tag}){note}")
        print("Listening… Ctrl+C to stop\n")

        for ch in active:
            t = threading.Thread(
                target=self._reader, args=(ch,), name=f"rx-{ch.name}", daemon=True
            )
            self.threads.append(t)
            t.start()

        try:
            while not self.stop.is_set():
                time.sleep(0.2)
        except KeyboardInterrupt:
            print("\nStopping…")
            self.stop.set()

        for t in self.threads:
            t.join(timeout=2.0)
        if self.log_fp:
            self.log_fp.close()
            print(f"Saved: {log_path}")

    def _write_line(self, line: str) -> None:
        with PRINT_LOCK:
            print(line, flush=True)
            if self.log_fp:
                self.log_fp.write(line + "\n")
                self.log_fp.flush()

    def _emit_sys(self, ch: ChannelCfg, message: str) -> None:
        tag = TX_TAG[ch.tx_from]
        self._write_line(f"{fmt_ts()}  [{ch.name:8}]  {tag:4}  SYS  {message}")

    def _emit(self, ch: ChannelCfg, data: bytes) -> None:
        tag = TX_TAG[ch.tx_from]
        parts = [f"{fmt_ts()}  [{ch.name:8}]  {tag:4}  RAW  {len(data)}B"]
        if self.cfg.show_hex:
            parts.append(data.hex(" "))
        if self.cfg.show_ascii:
            parts.append(ascii_preview(data))
        # hex первым (главное), ascii — подсказка
        if self.cfg.show_hex and self.cfg.show_ascii:
            line = f"{parts[0]}  |  {parts[1]}  |  {parts[2]}"
        elif self.cfg.show_hex:
            line = f"{parts[0]}  |  {parts[1]}"
        elif self.cfg.show_ascii:
            line = f"{parts[0]}  |  {parts[1]}"
        else:
            line = parts[0]
        self._write_line(line)

    def _reader(self, ch: ChannelCfg) -> None:
        baud = ch.baud if ch.baud is not None else self.cfg.baud
        flush_s = self.cfg.flush_timeout_ms / 1000.0
        chunker = RawChunker(flush_s)
        try:
            ser = serial.Serial(
                port=ch.port,
                baudrate=baud,
                bytesize=bytesize_const(self.cfg.bytesize),
                parity=parity_const(self.cfg.parity),
                stopbits=stopbits_const(self.cfg.stopbits),
                timeout=0.05,
            )
        except serial.SerialException as e:
            self._emit_sys(ch, f"OPEN_FAIL: {e}")
            return

        with ser:
            self._emit_sys(ch, f"OPEN {ch.port}@{baud} tx_from={ch.tx_from}")
            while not self.stop.is_set():
                try:
                    chunk = ser.read(256)
                except serial.SerialException as e:
                    self._emit_sys(ch, f"READ_ERR: {e}")
                    break
                if chunk:
                    for frame in chunker.feed(chunk):
                        self._emit(ch, frame)
                for frame in chunker.poll_flush():
                    self._emit(ch, frame)


def cmd_list_ports() -> int:
    ports = list(list_ports.comports())
    if not ports:
        print("No serial ports found")
        return 1
    print("Available ports:")
    for p in ports:
        print(f"  {p.device:8}  {p.description}  [{p.hwid}]")
    return 0


def run_self_test(config_path: Path | None = None) -> int:
    failed = 0

    def check(name: str, cond: bool, detail: str = "") -> None:
        nonlocal failed
        if cond:
            print(f"  OK  {name}")
        else:
            failed += 1
            extra = f" — {detail}" if detail else ""
            print(f"  FAIL  {name}{extra}")

    print("Self-test: raw chunker")
    c = RawChunker(0.01)
    check("accumulate", c.feed(b"ABC") == [])
    time.sleep(0.03)
    check("flush on gap", c.poll_flush() == [b"ABC"])

    c2 = RawChunker(0.05)
    big = b"X" * (MAX_CHUNK + 10)
    out = c2.feed(big)
    check("split max", out == [b"X" * MAX_CHUNK] and c2.poll_flush() == [] and len(c2.buf) == 10)

    check("ascii printable", ascii_preview(b"AB") == "AB")
    check(
        "ascii binary",
        ascii_preview(bytes([0x02, 0x03, 0x0D])) == "\\x02\\x03\\x0D",
    )
    check("tx alias ctrl", normalize_tx_from("ctrl", 0) == TX_CONTROLLER)
    check("tx alias actuator", normalize_tx_from("actuator", 0) == TX_DEVICE)
    try:
        normalize_tx_from(None, 0)
        check("tx_from required", False)
    except ConfigError:
        check("tx_from required", True)

    path = config_path or DEFAULT_CONFIG
    print(f"Self-test: config ({path})")
    if not path.is_file():
        check("config exists", False, str(path))
        return 1
    try:
        cfg = load_config(path)
        check("config load", True)
    except ConfigError as e:
        check("config load", False, str(e))
        return 1

    check("baud > 0", cfg.baud > 0)
    enabled = [c for c in cfg.channels if c.enabled]
    check("has channels", len(cfg.channels) > 0)
    check("has enabled channel", len(enabled) > 0, "enable at least one channel")
    for ch in enabled:
        check(f"tx_from {ch.name}", ch.tx_from in (TX_CONTROLLER, TX_DEVICE))

    print("Self-test: open enabled ports (read-only)")
    for ch in enabled:
        baud = ch.baud if ch.baud is not None else cfg.baud
        try:
            with serial.Serial(
                port=ch.port,
                baudrate=baud,
                bytesize=bytesize_const(cfg.bytesize),
                parity=parity_const(cfg.parity),
                stopbits=stopbits_const(cfg.stopbits),
                timeout=0.2,
            ) as ser:
                waiting = ser.in_waiting
                n = len(ser.read(64))
                check(f"open {ch.name}/{ch.port}@{baud}", True)
                print(f"       in_waiting={waiting} read={n}B tx_from={ch.tx_from}")
        except serial.SerialException as e:
            check(f"open {ch.name}/{ch.port}@{baud}", False, str(e))

    print()
    if failed:
        print(f"Self-test FAILED ({failed} check(s))")
        return 1
    print("Self-test OK")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="RS422/RS485 multi-channel raw sniffer")
    ap.add_argument(
        "--config",
        type=Path,
        default=DEFAULT_CONFIG,
        help=f"JSON config (default: {DEFAULT_CONFIG})",
    )
    ap.add_argument("--list-ports", action="store_true", help="List COM ports and exit")
    ap.add_argument(
        "--self-test",
        action="store_true",
        help="Run chunker/config/port checks and exit",
    )
    args = ap.parse_args()

    if args.list_ports:
        return cmd_list_ports()

    if args.self_test:
        return run_self_test(args.config)

    if not args.config.is_file():
        print(f"Config not found: {args.config}", file=sys.stderr)
        return 1

    try:
        cfg = load_config(args.config)
    except ConfigError as e:
        print(f"Config error: {e}", file=sys.stderr)
        return 1

    Sniffer(cfg, args.config).start()
    return 0


if __name__ == "__main__":
    sys.exit(main())
