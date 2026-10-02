#!/usr/bin/env python3
"""
Generate a C++ font table from Nextion .zi resources inside a .HMI file.

Uses the binary container parser from Nextion2Text.py (Max Zuidberg, MPL-2.0).
Glyph height is ZI header byte 0x07 (Character height).
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from typing import Dict, List, Optional, Sequence

_SCRIPT_DIR = Path(__file__).resolve().parent
if str(_SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPT_DIR))

from Nextion2Text import HMI  # noqa: E402

_ZI_MAGIC = b"\x04\xff\x00"
_ZI_WIDTH_OFF = 0x06
_ZI_HEIGHT_OFF = 0x07
_ZI_VERSION_OFF = 0x10


def _cpp_string_literal(value: str) -> str:
    escaped = (
        value.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\r", "\\r")
        .replace("\n", "\\n")
        .replace("\t", "\\t")
    )
    return f'"{escaped}"'


_BLOCK_RE = re.compile(
    r"// GENERATED-HMI-BEGIN:fonts\r?\n.*?// GENERATED-HMI-END:fonts",
    re.DOTALL,
)
_NAMESPACE_CLOSE_RE = re.compile(r"\r?\n\} // namespace hmi\r?\n")


def _font_name(blob: bytes) -> str:
    limit = min(len(blob), 0x80)
    for start in range(0x1C, limit):
        if not (0x20 <= blob[start] < 0x7F):
            continue
        end = blob.find(b"\x00", start, start + 64)
        if end <= start:
            continue
        raw = blob[start:end]
        if all(0x20 <= c < 0x7F for c in raw) and len(raw) >= 2:
            return raw.decode("ascii").strip()
    return ""


def extract_fonts(hmi: HMI) -> List[Dict[str, object]]:
    found: List[Dict[str, object]] = []
    for obj in hmi.header.content:
        if not obj.name.endswith(".zi"):
            continue
        stem = obj.name[:-3]
        if not stem.isdigit():
            continue
        blob = hmi.raw[obj.start : obj.start + obj.size]
        if len(blob) <= _ZI_VERSION_OFF or blob[:3] != _ZI_MAGIC:
            raise ValueError(f"{obj.name}: not a ZI font")
        name = _font_name(blob) or obj.name
        found.append(
            {
                "id": int(stem),
                "name": name,
                "height": blob[_ZI_HEIGHT_OFF],
                "width": blob[_ZI_WIDTH_OFF],
                "version": blob[_ZI_VERSION_OFF],
            }
        )
    found.sort(key=lambda font: int(font["id"]))
    return found


def render_block(hmi_name: str, fonts: Sequence[Dict[str, object]]) -> str:
    """Font table inside namespace nex::hmi. Struct Font lives in nexHmiSync.hpp."""
    rows: List[str] = []
    for font in fonts:
        width = font["width"]
        width_text = "variable" if width == 0 else str(width)
        rows.append(
            "    {"
            f"{font['id']}u, {font['height']}u, {_cpp_string_literal(str(font['name']))}"
            "}, "
            f"// ZI v{font['version']}, width {width_text}"
        )
    body = "\n".join(rows)
    return "\n".join(
        [
            "// GENERATED-HMI-BEGIN:fonts",
            f"// {hmi_name}",
            "inline constexpr Font kFonts[] = {",
            body,
            "};",
            "// GENERATED-HMI-END:fonts",
        ]
    )


def splice_fonts(content: str, block: str) -> str:
    newline = "\r\n" if "\r\n" in content else "\n"
    if newline == "\r\n":
        block = block.replace("\n", "\r\n")
    if _BLOCK_RE.search(content):
        return _BLOCK_RE.sub(block, content, count=1)
    matches = list(_NAMESPACE_CLOSE_RE.finditer(content))
    if not matches:
        raise ValueError("nexHmiConfig.hpp has no '} // namespace hmi' to append fonts")
    insert_at = matches[-1].start()
    return content[:insert_at] + newline + block + newline + content[insert_at:]


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate a C++ font id/height table from Nextion .zi resources in a .HMI file.",
    )
    parser.add_argument("-i", "--input", type=Path, required=True, help="Path to .HMI")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="nexHmiConfig.hpp to patch (default: src/UI/nexHmiConfig.hpp)",
    )
    parser.add_argument(
        "--stdout",
        action="store_true",
        help="Print result to stdout instead of writing a file",
    )
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    hmi_path = args.input.resolve()
    if not hmi_path.is_file():
        print(f"error: HMI not found: {hmi_path}", file=sys.stderr)
        return 1

    output_path = args.output if args.output is not None else Path("src/UI/nexHmiConfig.hpp")
    output_path = output_path.resolve()
    if not output_path.is_file():
        print(f"error: config not found: {output_path}", file=sys.stderr)
        return 1

    try:
        fonts = extract_fonts(HMI(str(hmi_path)))
    except Exception as exc:
        print(f"error: failed to read fonts: {exc}", file=sys.stderr)
        return 1

    if not fonts:
        print(f"error: no .zi fonts in {hmi_path.name}", file=sys.stderr)
        return 1

    block = render_block(hmi_path.name, fonts)
    existing = output_path.read_text(encoding="utf-8")
    try:
        text = splice_fonts(existing, block)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    if args.stdout:
        sys.stdout.write(text)
    else:
        output_path.write_text(text, encoding="utf-8", newline="")
        print(f"Updated {output_path} ({len(fonts)} font(s), source {hmi_path.name})", file=sys.stderr)

    for font in fonts:
        print(
            f"  id {font['id']}: {font['name']!r}  height {font['height']} px",
            file=sys.stderr,
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
