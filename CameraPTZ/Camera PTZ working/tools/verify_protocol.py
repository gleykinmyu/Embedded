#!/usr/bin/env python3
"""
Сверка каталогов проекта с ConvertibleProtocol.pdf v3.05.

Subcode в кадре — два hex-символа ASCII (Pattern 5/7/9–18): QSA:70 = байт 0x70,
не десятичное «70». Скрипт ищет все варианты регистра в тексте PDF.

Запуск: python tools/verify_protocol.py
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PDF = ROOT / "ConvertibleProtocol.pdf"
CATALOG = ROOT / "lib" / "ccam" / "include" / "catalog"
LIB_CCAM = ROOT / "lib" / "ccam"


def read_pdf_text() -> str:
    if not PDF.is_file():
        return ""
    data = PDF.read_bytes()
    chunks: list[str] = []
    current: list[str] = []
    for b in data:
        c = chr(b) if 32 <= b < 127 else "\n"
        if c == "\n":
            if len(current) >= 3:
                chunks.append("".join(current))
            current = []
        else:
            current.append(c)
    if current:
        chunks.append("".join(current))
    return "\n".join(chunks)


def subcode_ascii_hex(byte_val: int) -> tuple[str, str]:
    """
    Два hex-символа subcode в кадре (Pattern 5/7/9–18).

    Байт 0x70 -> '7','0' -> QSA:70 (не десятичное 70).
  """
    return (f"{byte_val:02X}", f"{byte_val:02x}")


# PDF v3.05: ожидаемые подстроки в ITEM (стр. 14–15 video, 9 OSD)
PDF_VIDEO_ITEM_HINTS: dict[int, tuple[str, ...]] = {
    0x60: ("MODE", "S.GAIN"),
    0x70: ("SCAN", "REVERSE"),
    0x71: ("FRAME", "RATE"),
}

PDF_OSD_ITEM_HINTS: dict[int, tuple[str, ...]] = {
    0x70: ("ASPECT",),
    0x72: ("ATW", "SPEED"),
}


def pdf_has_menu_subcode(pdf: str, query_prefix: str, set_prefix: str, subcode: int) -> bool:
    """
    Проверка Qxx:NN / Oxx:NN в PDF.
    NN — hex nibbles в ASCII (0x70 -> '7','0' -> QSA:70), не decimal 70.
    """
    hi, lo = subcode_ascii_hex(subcode)
    needles: list[str] = []
    for q in (query_prefix, set_prefix):
        for nn in (hi, lo):
            needles.append(f"{q}:{nn}")
            needles.append(f"{q} :{nn}")
            needles.append(f"{q}: {nn}")
    pdf_upper = pdf.upper()
    return any(n.upper() in pdf_upper for n in needles)


def pdf_has_token(pdf: str, token: str) -> bool:
    if not token:
        return True
    return token.upper() in pdf.upper()


def parse_camera_commands(text: str) -> list[tuple[str, str, str]]:
    m = re.search(
        r"inline constexpr CameraCommandEntry kCameraCommands\[\] = \{(.*?)\};",
        text,
        re.S,
    )
    if not m:
        return []
    rows = []
    for line in m.group(1).splitlines():
        line = line.strip().rstrip(",")
        if not line.startswith("{"):
            continue
        parts = [p.strip().strip('"') for p in line.strip("{}").split(",")]
        if len(parts) >= 3:
            rows.append((parts[1], parts[2], parts[0]))
    return rows


def parse_pt_commands(text: str) -> list[tuple[str, str]]:
    m = re.search(
        r"inline constexpr PtCommandEntry kPtCommands\[\] = \{(.*?)\};",
        text,
        re.S,
    )
    if not m:
        return []
    rows = []
    for line in m.group(1).splitlines():
        line = line.strip().rstrip(",")
        if not line.startswith("{"):
            continue
        parts = [p.strip().strip('"') for p in line.strip("{}").split(",")]
        if len(parts) >= 2:
            rows.append((parts[1], parts[0]))
    return rows


def parse_menu_subcodes(text: str, table: str) -> list[tuple[int, str]]:
    m = re.search(
        rf"inline constexpr MenuSubcodeEntry {table}\[\] = \{{(.*?)\}};",
        text,
        re.S,
    )
    if not m:
        return []
    rows = []
    for line in m.group(1).splitlines():
        line = line.strip().rstrip(",")
        if not line.startswith("{"):
            continue
        hm = re.search(r"\{0x([0-9A-Fa-f]+),\s*\"([^\"]+)\"\}", line)
        if hm:
            rows.append((int(hm.group(1), 16), hm.group(2)))
    return rows


def check_enum_count(header: str, enum: str, table_var: str) -> list[str]:
    issues: list[str] = []
    em = re.search(rf"enum class {enum}\s*:\s*uint8_t\s*\{{(.*?)Count\s*\}};", header, re.S)
    if not em:
        return [f"{enum}: enum not found"]
    items = [x.strip() for x in em.group(1).split(",") if x.strip() and x.strip() != "Count"]
    sub = re.search(
        rf"inline constexpr MenuSubcodeEntry {table_var}\[\] = \{{(.*?)\}};",
        header,
        re.S,
    )
    table_n = len(re.findall(r"\{0x", sub.group(1))) if sub else 0
    if len(items) != table_n:
        issues.append(f"{enum}: enum items={len(items)} table rows={table_n}")
    return issues


def scan_no_string_catalog_lookup() -> list[str]:
    issues: list[str] = []
    bad = re.compile(r"\b(strcmp|strstr|ptFindCommand|camFindCommand|findByName|applyByName)\b")
    for base in (
        LIB_CCAM / "src",
        LIB_CCAM / "include" / "devices",
        LIB_CCAM / "include" / "catalog",
        LIB_CCAM / "examples",
    ):
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if path.suffix not in (".cpp", ".hpp", ".h"):
                continue
            if "verify_protocol" in path.name:
                continue
            text = path.read_text(encoding="utf-8", errors="ignore")
            if bad.search(text):
                issues.append(f"String catalog lookup in {path.relative_to(ROOT)}")
    return issues


def main() -> int:
    cam_h = (CATALOG / "camera_commands.hpp").read_text(encoding="utf-8")
    pt_h = (CATALOG / "pt_commands.hpp").read_text(encoding="utf-8")
    osd_h = (CATALOG / "osd_menu.hpp").read_text(encoding="utf-8")
    vid_h = (CATALOG / "video_menu.hpp").read_text(encoding="utf-8")
    ext_h = (CATALOG / "extended_j_menu.hpp").read_text(encoding="utf-8")

    pdf = read_pdf_text()
    pdf_ok = bool(pdf)
    if pdf_ok:
        print(f"PDF text extracted: {len(pdf)} chars")
    else:
        print("WARN: PDF not readable; skipping PDF cross-check")

    issues: list[str] = []

    if "kCameraCommandCount == static_cast<size_t>(CameraCmd::Count)" not in cam_h:
        issues.append("camera_commands: missing CameraCmd Count static_assert")
    if "kPtCommandCount == static_cast<size_t>(PtCmd::Count)" not in pt_h:
        issues.append("pt_commands: missing PtCmd Count static_assert")

    issues.extend(check_enum_count(osd_h, "OsdItem", "kOsdMenu"))
    issues.extend(check_enum_count(vid_h, "VideoMenuItem", "kVideoMenu"))
    issues.extend(check_enum_count(ext_h, "ExtendedJItem", "kExtendedJMenu"))

    if "kSceneCommand.pattern == CameraPattern::Scene" not in cam_h:
        issues.append("camera_commands: missing kSceneCommand Scene static_assert")

    issues.extend(scan_no_string_catalog_lookup())

    cam = parse_camera_commands(cam_h)
    pt = parse_pt_commands(pt_h)
    osd = parse_menu_subcodes(osd_h, "kOsdMenu")
    vid = parse_menu_subcodes(vid_h, "kVideoMenu")
    ext = parse_menu_subcodes(ext_h, "kExtendedJMenu")

    if pdf_ok:
        spot = [
            ("QID", "OID"),
            ("QAW", "OAW"),
            ("QSH", "OSH"),
            ("QGU", "OGU"),
            ("XSF", None),
            ("#PTS", None),
            ("#APC", None),
        ]
        for q, o in spot:
            if not pdf_has_token(pdf, q):
                issues.append(f"PDF missing token: {q}")
            if o and not pdf_has_token(pdf, o):
                issues.append(f"PDF missing token: {o}")

        # Subcode spot-checks (hex ASCII in frame)
        subcode_checks = [
            ("QSD", "OSD", 0x72, "ATW Speed"),
            ("QSA", "OSA", 0x60, "MODE @S.GAIN"),
            ("QSA", "OSA", 0x70, "SCAN REVERSE"),
            ("QSJ", "OSJ", 0x26, "HDMI HDR"),
            ("QSJ", "OSJ", 0x3D, "Zoom Scale"),
            ("QSJ", "OSJ", 0x40, "Operation Lock"),
        ]
        for q, o, code, label in subcode_checks:
            if not pdf_has_menu_subcode(pdf, q, o, code):
                hi, _ = subcode_ascii_hex(code)
                issues.append(
                    f"PDF missing {label}: {q}:{hi} / {o}:{hi} (subcode 0x{code:02X})"
                )

        for q, ctrl, name in cam:
            if q and not pdf_has_token(pdf, q) and not pdf_has_token(pdf, ctrl):
                issues.append(f"Camera '{name}': neither {q!r} nor {ctrl!r} in PDF")

        for ctrl, name in pt:
            token = ctrl if ctrl.startswith("#") else f"#{ctrl}"
            if not pdf_has_token(pdf, token) and not pdf_has_token(pdf, ctrl):
                issues.append(f"PT '{name}': {token!r} not in PDF")

        for code, name in osd:
            if not pdf_has_menu_subcode(pdf, "QSD", "OSD", code):
                issues.append(f"OSD '{name}' subcode 0x{code:02X}: not in PDF")
            hints = PDF_OSD_ITEM_HINTS.get(code)
            if hints and not all(h.upper() in name.upper() for h in hints):
                issues.append(
                    f"OSD catalog 0x{code:02X} '{name}' "
                    f"does not match PDF hints {hints}"
                )

        for code, name in vid:
            if not pdf_has_menu_subcode(pdf, "QSA", "OSA", code):
                issues.append(f"Video '{name}' subcode 0x{code:02X}: not in PDF")
            hints = PDF_VIDEO_ITEM_HINTS.get(code)
            if hints and not all(h.upper() in name.upper() for h in hints):
                issues.append(
                    f"Video catalog 0x{code:02X} '{name}' "
                    f"does not match PDF hints {hints}"
                )

        for code, name in ext:
            if not pdf_has_menu_subcode(pdf, "QSJ", "OSJ", code):
                issues.append(f"ExtendedJ '{name}' subcode 0x{code:02X}: not in PDF")

    he130 = (
        ROOT / "lib" / "ccam" / "include" / "devices" / "models" / "he130.hpp"
    ).read_text(
        encoding="utf-8"
    )
    if "VideoMenuItem::ModeAtSuperGain" not in he130:
        issues.append("He130: Super Gain -> VideoMenuItem::ModeAtSuperGain (QSA:60 / 0x60)")
    if "OsdItem::AtwSpeed" not in he130:
        issues.append("He130: ATW Speed -> OsdItem::AtwSpeed (QSD:72 / 0x72)")
    if "ExtendedJItem::HdmiOutHdrOutputSelect" not in he130:
        issues.append("He130: HDR -> ExtendedJItem::HdmiOutHdrOutputSelect (QSJ:26)")

    print(
        f"Catalog: Camera={len(cam)} PT={len(pt)} "
        f"OSD={len(osd)} Video={len(vid)} ExtendedJ={len(ext)}"
    )

    if issues:
        print(f"\nISSUES ({len(issues)}):")
        for i in issues[:40]:
            print(f"  - {i}")
        if len(issues) > 40:
            print(f"  ... and {len(issues) - 40} more")
        return 1

    print("\nOK: catalogs, enum/table pairing, and PDF subcode checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
