#!/usr/bin/env python3
"""Собрать www → status.html (+ src/generated для EMBED_TXTFILES)."""

from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

subprocess.check_call(
    [
        sys.executable,
        str(ROOT / "tools" / "build_www.py"),
        "--out",
        str(ROOT / "src" / "generated" / "status.html"),
    ],
    cwd=str(ROOT),
)
