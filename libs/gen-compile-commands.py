#!/usr/bin/env python3
"""Один compile_commands.json на весь libs/ для clangd (host clang++).

Заголовки/исходники libs не входят в PlatformIO compiledb при правке «снаружи»
проекта. Перегенерация:

  python3 libs/gen-compile-commands.py

Исключения (свой тулчейн / своя база):
  board/mega2560  — AVR, gen-compile-commands.ps1
  dmx             — .clangd → ../../DMX (опционально)
  Nextion/examples, Nextion/syncHMI
"""
from __future__ import annotations

import json
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
    """Типичные -I как у PlatformIO lib_extra_dirs + layout библиотек."""
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
        LIBS / "ccam" / "src",
    ]
    return [str(d) for d in dirs if d.is_dir()]


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
    file_s = str(path.resolve())
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
    args.append(file_s)
    return {
        "directory": str(LIBS),
        "file": file_s,
        "arguments": args,
    }


def main() -> int:
    cxx = shutil.which("clang++") or shutil.which("c++")
    if not cxx:
        print("clang++/c++ не найден в PATH", file=sys.stderr)
        return 1
    cxx = str(Path(cxx).resolve())

    files = collect_files()
    if not files:
        print(f"В {LIBS} нет исходников", file=sys.stderr)
        return 1

    incs = include_dirs()
    entries = [entry_for(p, cxx, incs) for p in files]

    out = LIBS / "compile_commands.json"
    out.write_text(json.dumps(entries, indent=4) + "\n", encoding="utf-8")
    print(f"clang++: {cxx}")
    print(f"includes: {len(incs)}")
    print(f"Wrote {len(entries)} entries: {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
