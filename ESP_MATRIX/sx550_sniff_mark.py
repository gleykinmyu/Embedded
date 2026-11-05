#!/usr/bin/env python3
"""Append a MARK line for the running dual sniffer."""
import sys
from pathlib import Path

mark = Path(__file__).with_name("sniff_mark.txt")
text = " ".join(sys.argv[1:]).strip() or " unmarked "
with mark.open("a", encoding="utf-8") as f:
    f.write(text + "\n")
print(f"marked: {text}")
