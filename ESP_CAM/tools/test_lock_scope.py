"""Проверяет, что DeviceLock не удерживается на время HTTP, malloc, паузы и printf.

Без устройства и без прошивки. Запуск: python tools/test_lock_scope.py
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FILES = [
    ROOT / "src" / "http_status.cpp",
    ROOT / "src" / "cli_eth01.cpp",
    ROOT / "src" / "ph350_ctrl.cpp",
]

FORBIDDEN = (
    "send_json",
    "httpd_resp_send",
    "httpd_resp_send_chunk",
    "malloc",
    "printf",
    "putchar",
    "vTaskDelay",
)


def code_only(line: str) -> str:
    line = re.sub(r"/\*.*?\*/", "", line)
    if "//" in line:
        line = line[: line.index("//")]
    return line


def violations(path: Path) -> list[str]:
    depth = 0
    locks: list[int] = []
    found: list[str] = []
    for lineno, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = code_only(raw)
        for ch in line:
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                while locks and locks[-1] > depth:
                    locks.pop()
        if "DeviceLock" in line and "::DeviceLock" not in line and "class " not in line:
            locks.append(depth)
        if not locks:
            continue
        for token in FORBIDDEN:
            if re.search(rf"\b{token}\b", line):
                found.append(f"{path.name}:{lineno}: {token} under DeviceLock")
    return found


def main() -> int:
    bad: list[str] = []
    for path in FILES:
        bad.extend(violations(path))
    if bad:
        print("FAIL")
        for item in bad:
            print(item)
        return 1
    print(f"PASS lock scope ({len(FILES)} files)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
