#!/usr/bin/env python3
"""Один compile_commands.json на весь libs/ для clangd (host clang++).

Заголовки/исходники libs не входят в PlatformIO compiledb при правке «снаружи»
проекта. Перегенерация:

  python3 libs/gen-compile-commands.py

file и -I — относительно libs/ (directory — абсолютный путь к libs: clangd 22
склеивает относительный file с directory и не раскрывает directory «.»).

Исключения (свой тулчейн / своя база):
  board/mega2560  — AVR, gen-compile-commands.ps1
  dmx             — .clangd → ../../DMX (опционально)
  Nextion/examples, Nextion/syncHMI
"""
from __future__ import annotations

import json
import os
import shutil
import sys
from pathlib import Path

LIBS = Path(__file__).resolve().parent

# Относительные пути от libs/, которые не кладём в общую базу
SKIP_PREFIXES = (
    "board/mega2560/",
    "Nextion/examples/",
    "Nextion/syncHMI/",
)

SRC_SUFFIXES = {".c", ".cc", ".cpp", ".cxx"}
HDR_SUFFIXES = {".h", ".hh", ".hpp", ".hxx"}


def skipped(rel: Path) -> bool:
    s = rel.as_posix()
    if any(part.startswith(".") for part in rel.parts):
        return True
    return any(s.startswith(p) or s == p.rstrip("/") for p in SKIP_PREFIXES)


def include_dirs() -> list[str]:
    """-I относительно libs/ (как lib_extra_dirs + layout библиотек)."""
    dirs = [
        LIBS,  # #include "mem/slot.hpp", "smcp/ilink.hpp"
        LIBS / "Interfaces",
        LIBS / "smcp",
        LIBS / "Nextion",
        LIBS / "showFile",
        LIBS / "debug",
        LIBS / "dmx",
        LIBS / "FatFs",
        LIBS / "board",
        LIBS / "mem",
        LIBS / "ccam" / "include",
        LIBS / "bitsery" / "include",  # <bitsery/...>
        LIBS / "ccam" / "src",
    ]
    rels: list[str] = []
    for d in dirs:
        if not d.is_dir():
            continue
        rel = d.resolve().relative_to(LIBS).as_posix()
        rels.append("." if rel == "." or rel == "" else rel)
    return rels


def collect_files() -> list[Path]:
    files: list[Path] = []
    for path in LIBS.rglob("*"):
        if not path.is_file():
            continue
        rel = path.relative_to(LIBS)
        if skipped(rel):
            continue
        if path.suffix.lower() in SRC_SUFFIXES | HDR_SUFFIXES:
            files.append(path)
    return sorted(files)


def entry_for(path: Path, cxx: str, incs: list[str]) -> dict:
    rel = path.resolve().relative_to(LIBS).as_posix()
    suf = path.suffix.lower()
    args = [cxx]
    if suf in HDR_SUFFIXES:
        args += ["-x", "c-header" if suf == ".h" else "c++-header"]
    elif suf == ".c":
        args += ["-x", "c", "-std=c11"]
    else:
        args += ["-std=c++17"]
    args += ["-Wall", "-Wextra"]
    for inc in incs:
        args.append(f"-I{inc}")
    args.append(rel)
    return {
        # Абсолютный каталог libs: относительно него заданы file и -I.
        "directory": LIBS.as_posix(),
        "file": rel,
        "arguments": args,
    }


def arm_gxx() -> str | None:
    """Тулчейн PlatformIO: clangd через --query-driver берёт у него <cstdint>."""
    found = shutil.which("arm-none-eabi-g++") or shutil.which("arm-none-eabi-g++.exe")
    if found:
        # Не resolve(): g++ — симлинк на c++, а --query-driver ловит имя g++.
        return str(Path(found).absolute())
    roots = []
    if os.environ.get("PLATFORMIO_CORE_DIR"):
        roots.append(Path(os.environ["PLATFORMIO_CORE_DIR"]))
    roots.append(Path.home() / ".platformio")
    roots.append(Path("C:/.platformio"))
    for root in roots:
        packages = root / "packages"
        if not packages.is_dir():
            continue
        hits = sorted(packages.glob("toolchain-gccarmnoneeabi*/bin/arm-none-eabi-g++*"))
        exe = [p for p in hits if p.suffix.lower() == ".exe"]
        pick = exe[-1] if exe else (hits[-1] if hits else None)
        if pick and pick.is_file():
            return str(pick.absolute())
    return None


def main() -> int:
    # arm-none-eabi-g++ этой машины, иначе имя clang++ (на macOS он несёт libc++).
    cxx = arm_gxx() or "clang++"
    if cxx != "clang++":
        cxx = Path(cxx).as_posix()

    files = collect_files()
    if not files:
        print(f"В {LIBS} нет исходников", file=sys.stderr)
        return 1

    incs = include_dirs()
    entries = [entry_for(p, cxx, incs) for p in files]

    out = LIBS / "compile_commands.json"
    out.write_text(json.dumps(entries, indent=4) + "\n", encoding="utf-8")
    print(f"compiler: {cxx}")
    print(f"includes: {len(incs)}")
    print(f"Wrote {len(entries)} entries: {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
