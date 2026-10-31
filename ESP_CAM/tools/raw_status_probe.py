#!/usr/bin/env python3
"""Прогон status/query команд из Convertible Protocol через /api/ptz?cmd=raw."""

from __future__ import annotations

import argparse
import json
import urllib.parse
import urllib.request

# Confirmation / Get из PDF (стр. 30–32) + типичные запросы статуса для PH/HE.
# payload=None → только cmd (как Query/Confirmation без данных).
QUERIES: list[tuple[str, str | None, str]] = [
    ("O", None, "Power status"),
    ("RER", None, "Error Status Info"),
    ("RID", "0", "ID query (#RID[Data])"),
    ("RID", "1", "ID unit1"),
    ("QSV", "0", "Software version unit0"),
    ("QSV", "1", "Software version unit1"),
    ("QSV", "2", "Software version unit2"),
    ("PST", "0", "Preset table"),
    ("PE", "0", "Preset Entry confirm #PE0"),
    ("PE", "1", "Preset Entry #PE1"),
    ("GZ", None, "Get Zoom D/A"),
    ("GF", None, "Get Focus D/A"),
    ("GI", None, "Get Iris D/A"),
    ("APC", None, "Abs PT position query"),
    ("AXZ", None, "Zoom pos confirm"),
    ("AXF", None, "Focus pos confirm"),
    ("AXI", None, "Iris pos confirm"),
    ("PTG", None, "Get Gain/CT/Shutter/ND"),
    ("PTV", None, "Get PTZFI (hex)"),
    ("PTD", None, "Get PTZFI (dec)"),
    ("TAA", None, "Tally information"),
    ("PD", "0", "PinP Position"),
    ("GDL", "0", "Guide Line"),
    ("LPC", "0", "Limit? LPC"),
    ("LPI", None, "LPI query"),
    ("DS", None, "DS query"),
    ("FS1", None, "FS1"),
    ("HS", None, "HS"),
    ("V", None, "V"),
    ("INS", "0", "INS"),
    ("CMP", "0", "CMP"),
    ("FDA", "0", "FDA"),
    ("SPF", "0", "SPF"),
    ("OPT", "0", "Option"),
    ("WLC", "0", "Wireless"),
    ("CSV", "0", "CSV (control form)"),
]


def call(host: str, cmd: str, payload: str | None, timeout: float) -> dict:
    q = {"cmd": "raw", "data": cmd}
    if payload is not None:
        q["data2"] = payload
    url = f"http://{host}/api/ptz?" + urllib.parse.urlencode(q)
    with urllib.request.urlopen(url, timeout=timeout) as r:
        return json.loads(r.read().decode())


ROUND2: list[tuple[str, str | None, str]] = [
    ("O", "1", "Power with data1"),
    ("S", None, "Latest preset?"),
    ("S", "01", "Recall/confirm P01"),
    ("L1", None, "Limit TiltUp"),
    ("L2", None, "Limit TiltDown"),
    ("L3", None, "Limit PanLeft"),
    ("L4", None, "Limit PanRight"),
    ("APC", "80008000", "APC go/query center"),
    ("PE", "00", "PE00"),
    ("PE", "01", "PE01"),
    ("RID", None, "RID no data"),
    ("QSV", None, "QSV no data"),
    ("SPF", None, "SPF no data"),
    ("SPF", "1", "SPF1"),
    ("GDL", None, "GDL no data"),
    ("GDL", "1", "GDL1"),
    ("D1", None, "D1 Extender/AF"),
    ("D1", "0", "D1=0"),
    ("RT", None, "RT"),
    ("UPVS", None, "UPVS"),
    ("PST", None, "PST no data"),
    ("OPT", None, "OPT no data"),
]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="192.168.6.221")
    ap.add_argument("--timeout", type=float, default=3.0)
    ap.add_argument("--round2", action="store_true")
    args = ap.parse_args()

    items = ROUND2 if args.round2 else QUERIES
    print(f"host={args.host}  n={len(items)}")
    print(f"{'cmd':<8} {'pay':<6} {'ok':<5} {'status':<10} data")
    print("-" * 72)
    ok_n = fail_n = 0
    for cmd, pay, note in items:
        try:
            d = call(args.host, cmd, pay, args.timeout)
        except Exception as e:
            print(f"#{cmd:<7} {pay or '':<6} ERR   net        {e}  [{note}]")
            fail_n += 1
            continue
        ok = bool(d.get("ok"))
        st = str(d.get("status", "?"))
        data = d.get("data") or ""
        if isinstance(data, str) and len(data) > 48:
            data = data[:45] + "..."
        mark = "OK" if ok else "FAIL"
        if ok:
            ok_n += 1
        else:
            fail_n += 1
        print(f"#{cmd:<7} {pay or '':<6} {mark:<5} {st:<10} {data}  [{note}]")
    print("-" * 72)
    print(f"OK={ok_n} FAIL={fail_n}")


if __name__ == "__main__":
    main()
