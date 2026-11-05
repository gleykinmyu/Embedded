#!/usr/bin/env python3
"""
Проверка связи с Panasonic WJ-SX550(A/B/C) по RS-232.

Параметры порта (проверено на железе WJ-SX550):
  9600 baud, 8 data, odd parity, 1 stop, без RTS/CTS
  (в мануале для PC указано 7O2+RS/CS — на практике ответил 8O1)

Кадр (DNA200 / семейство Panasonic matrix):
  STX (0x02) + ASCII + ETX (0x03)
  ACK = 0x06

Примеры:
  python sx550_rs232_test.py --list
  python sx550_rs232_test.py --port COM11
  python sx550_rs232_test.py --port COM11 --probe
  python sx550_rs232_test.py --port COM11 --login
  python sx550_rs232_test.py --port COM11 --cam 1 --mon 1
"""

from __future__ import annotations

import argparse
import sys
import time
from typing import Optional

import serial
import serial.tools.list_ports

STX = b"\x02"
ETX = b"\x03"
ACK = 0x06
NAK = 0x15

DEFAULT_USER = "1"
DEFAULT_PASSWORD = "12345"


def list_ports() -> None:
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("COM-порты не найдены.")
        return
    print("Доступные порты:")
    for p in ports:
        print(f"  {p.device:8}  {p.description}")
        if p.hwid:
            print(f"           {p.hwid}")


def hexdump(data: bytes) -> str:
    if not data:
        return "(пусто)"
    hexs = " ".join(f"{b:02X}" for b in data)
    ascii_ = "".join(chr(b) if 32 <= b < 127 else "." for b in data)
    return f"{hexs}  | {ascii_}"


def build_frame(body: str) -> bytes:
    return STX + body.encode("ascii") + ETX


def open_port(
    port: str,
    baud: int = 9600,
    bytesize: int = 7,
    parity: str = "O",
    stopbits: float = 2,
    rtscts: bool = True,
    timeout: float = 1.0,
) -> serial.Serial:
    ser = serial.Serial(
        port=port,
        baudrate=baud,
        bytesize=bytesize,
        parity=parity,
        stopbits=stopbits,
        rtscts=rtscts,
        xonxoff=False,
        dsrdtr=False,
        timeout=timeout,
        write_timeout=2.0,
    )
    # Многие USB-UART поднимают линии только после open
    ser.dtr = True
    ser.rts = True
    time.sleep(0.1)
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    return ser


def transact(ser: serial.Serial, body: str, wait_s: float = 1.0) -> bytes:
    frame = build_frame(body)
    print(f"TX  {body}")
    print(f"    {hexdump(frame)}")
    ser.write(frame)
    ser.flush()

    deadline = time.time() + wait_s
    buf = bytearray()
    while time.time() < deadline:
        chunk = ser.read(ser.in_waiting or 1)
        if chunk:
            buf.extend(chunk)
            # после ACK часто ещё идёт ответный кадр — чуть подождём
            time.sleep(0.05)
            if ser.in_waiting:
                continue
            # если есть полный ответ STX..ETX — можно выходить раньше
            if ACK in buf or NAK in buf:
                if STX in buf and ETX in buf and buf.rfind(ETX) > buf.find(STX):
                    break
                if len(buf) == 1 and buf[0] in (ACK, NAK):
                    time.sleep(0.15)
                    if not ser.in_waiting:
                        break
        else:
            time.sleep(0.02)

    print(f"RX  {hexdump(bytes(buf))}")
    if ACK in buf:
        print("    → ACK (0x06) получен")
    if NAK in buf:
        print("    → NAK (0x15) получен")
    if not buf:
        print("    → нет ответа (проверь порт/кабель/скорость/handshake)")
    print()
    return bytes(buf)


def login_variants(user: str, password: str) -> list[str]:
    """Варианты login: DNA200 (SX550A) и стиль SX650."""
    u = user.lstrip("0") or "0"
    variants = [
        f"CMD:W{u}-{password}",  # DNA200: CMD:Wi-p
        f"CMD:W{user.zfill(5)}-{password.zfill(5)}",  # SX650-like
        f"CMD:W{user}-{password}",
    ]
    # уникальные, порядок сохранить
    seen: set[str] = set()
    out: list[str] = []
    for v in variants:
        if v not in seen:
            seen.add(v)
            out.append(v)
    return out


def cam_variants(mon: int, cam: int) -> list[str]:
    return [
        f"CMD:H{mon:03d}CS{cam:04d}",  # DNA200
        f"CMD:H{mon:02d}CS{cam:03d}",  # SX650-like
        f"CMD:H{mon}CS{cam}",
    ]


def cmd_logout(mon: int = 1) -> list[str]:
    return [
        "CMD:HLG",
        f"CMD:H{mon:02d}LG",
    ]


def probe(ser: serial.Serial, user: str, password: str, mon: int, cam: int) -> None:
    print("=== PROBE: login → camera select → logout ===\n")
    ok_login: Optional[str] = None
    for body in login_variants(user, password):
        resp = transact(ser, body, wait_s=1.2)
        if ACK in resp:
            ok_login = body
            break
        time.sleep(0.2)

    if not ok_login:
        print("Login не подтверждён ACK. Пробую camera select без успешного login...\n")

    ok_cam: Optional[str] = None
    for body in cam_variants(mon, cam):
        resp = transact(ser, body, wait_s=1.2)
        if ACK in resp:
            ok_cam = body
            break
        time.sleep(0.2)

    for body in cmd_logout(mon):
        resp = transact(ser, body, wait_s=1.0)
        if ACK in resp:
            break
        time.sleep(0.2)

    print("=== Итог ===")
    print(f"login format : {ok_login or 'не найден'}")
    print(f"camera format: {ok_cam or 'не найден'}")


def interactive(ser: serial.Serial, user: str, password: str) -> None:
    print(
        "Интерактивный режим. Команды:\n"
        "  login              — перебор login\n"
        "  cam <mon> <cam>    — выбор камеры на монитор\n"
        "  logout [mon]       — выход\n"
        "  raw <ASCII>        — сырой body без STX/ETX (кадр добавится)\n"
        "  hex <AA BB ..>     — сырые байты как есть\n"
        "  read               — дочитать буфер\n"
        "  quit               — выход\n"
    )
    while True:
        try:
            line = input("> ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            break
        if not line:
            continue
        parts = line.split()
        cmd = parts[0].lower()

        if cmd in ("q", "quit", "exit"):
            break
        if cmd == "login":
            for body in login_variants(user, password):
                if ACK in transact(ser, body):
                    break
                time.sleep(0.2)
        elif cmd == "cam" and len(parts) >= 3:
            mon, cam = int(parts[1]), int(parts[2])
            for body in cam_variants(mon, cam):
                if ACK in transact(ser, body):
                    break
                time.sleep(0.2)
        elif cmd == "logout":
            mon = int(parts[1]) if len(parts) > 1 else 1
            for body in cmd_logout(mon):
                if ACK in transact(ser, body):
                    break
                time.sleep(0.2)
        elif cmd == "raw" and len(parts) >= 2:
            transact(ser, " ".join(parts[1:]))
        elif cmd == "hex" and len(parts) >= 2:
            data = bytes(int(x, 16) for x in parts[1:])
            print(f"TX  {hexdump(data)}")
            ser.write(data)
            ser.flush()
            time.sleep(0.5)
            rx = ser.read(ser.in_waiting or 64)
            print(f"RX  {hexdump(rx)}\n")
        elif cmd == "read":
            time.sleep(0.2)
            rx = ser.read(ser.in_waiting or 64)
            print(f"RX  {hexdump(rx)}\n")
        else:
            print("Неизвестная команда.")


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="WJ-SX550 RS-232 smoke test")
    p.add_argument("--list", action="store_true", help="Список COM-портов")
    p.add_argument("--port", help="COM-порт Waveshare (например COM11)")
    p.add_argument("--baud", type=int, default=9600)
    p.add_argument(
        "--mode",
        choices=("8O1", "7O2", "8N1"),
        default="8O1",
        help="Формат порта: 8O1 = рабочий на SX550, 7O2 = как в мануале PC, 8N1 = запасной",
    )
    p.add_argument("--rtscts", action="store_true", help="Включить аппаратный handshake RTS/CTS")
    p.add_argument("--no-rtscts", action="store_true", help="(устарело) handshake и так выключен по умолчанию")
    p.add_argument("--user", default=DEFAULT_USER)
    p.add_argument("--password", default=DEFAULT_PASSWORD)
    p.add_argument("--probe", action="store_true", help="Автопроверка login/cam/logout")
    p.add_argument("--login", action="store_true", help="Только login")
    p.add_argument("--logout", action="store_true")
    p.add_argument("--cam", type=int, help="Номер камеры")
    p.add_argument("--mon", type=int, default=1, help="Номер монитора")
    p.add_argument("-i", "--interactive", action="store_true")
    return p.parse_args()


def main() -> int:
    args = parse_args()
    if args.list or not args.port:
        list_ports()
        if not args.port:
            if args.list:
                return 0
            print("\nУкажи порт Waveshare, например:")
            print("  python sx550_rs232_test.py --port COM11 --probe")
            print("Если у конвертера несколько портов — PORTD обычно один из USB Serial.")
            return 1

    if args.mode == "8O1":
        bytesize, parity, stopbits = 8, serial.PARITY_ODD, serial.STOPBITS_ONE
    elif args.mode == "7O2":
        bytesize, parity, stopbits = 7, serial.PARITY_ODD, serial.STOPBITS_TWO
    else:
        bytesize, parity, stopbits = 8, serial.PARITY_NONE, serial.STOPBITS_ONE

    rtscts = bool(args.rtscts)
    print(
        f"Открываю {args.port}: {args.baud} {args.mode} rtscts={rtscts}"
    )
    try:
        ser = open_port(
            args.port,
            baud=args.baud,
            bytesize=bytesize,
            parity=parity,
            stopbits=stopbits,
            rtscts=rtscts,
        )
    except serial.SerialException as e:
        print(f"Не удалось открыть порт: {e}")
        return 2

    with ser:
        print(f"OK: {ser.port} open (DTR={ser.dtr}, RTS={ser.rts})\n")

        if args.probe:
            probe(ser, args.user, args.password, args.mon, args.cam or 1)
        elif args.login:
            for body in login_variants(args.user, args.password):
                if ACK in transact(ser, body):
                    break
                time.sleep(0.2)
        elif args.cam is not None:
            for body in login_variants(args.user, args.password):
                if ACK in transact(ser, body):
                    break
                time.sleep(0.2)
            for body in cam_variants(args.mon, args.cam):
                if ACK in transact(ser, body):
                    break
                time.sleep(0.2)
        elif args.logout:
            for body in cmd_logout(args.mon):
                if ACK in transact(ser, body):
                    break
                time.sleep(0.2)
        else:
            interactive(ser, args.user, args.password)

    return 0


if __name__ == "__main__":
    sys.exit(main())
