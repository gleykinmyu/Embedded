#!/usr/bin/env python3
"""
Прогон HTTP API ESP_CAM по caps выбранной модели.

  python tools/api_probe.py
  python tools/api_probe.py --host 192.168.6.221
  python tools/api_probe.py --safe          # только inquiry
  python tools/api_probe.py --no-live       # без pan/tilt hold
"""

from __future__ import annotations

import argparse
import json
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass, field
from typing import Any, Optional

DEFAULT_HOST = "192.168.6.221"
TIMEOUT_S = 15.0

# cmd → ключ в pt_caps / cam_caps (None = всегда)
PT_CAP: dict[str, Optional[str]] = {
    "pos": "pt_pos",
    "pos_disp": "pos_disp",
    "req_zoom": "lens_q",
    "req_focus": "lens_q",
    "req_iris": "lens_q",
    "req_lens": "lens_q",
    "set_zoom": "lens_q",
    "set_focus": "lens_q",
    "set_iris": "lens_q",
    "set_lens": "lens_q",
    "set_pt": "abs_pt",
    "preset_map": "preset_map",
    "limit_toggle": "limit_toggle",
    "limit_ctrl": "limit_ctrl",
    "tally": "tally",
    "wireless": "wireless",
    "washer": "washer",
    "option": "option",
    "fan": "fan",
    "fan2": "fan",
    "wiper": "wiper",
    "tilt_range": "tilt_range",
    "req_heater": "heater",
    "req_defrost": "defroster",
    "req_exposure": "exposure_q",
    "req_landing": "landing",
    "req_ver": "software_ver",
    "lens_ctrl": "lens_q",
}

CAM_CAP: dict[str, Optional[str]] = {
    "abc_start": "abc",
    "synchro": "synchro",
    "hd_detail": "hd_detail",
    "negpos": "neg_pos",
    "fieldframe": "field_frame",
    "chroma": "chroma",
    "black_shade": "black_shade",
    "r_gain": "paint",
    "b_gain": "paint",
    "r_ped": "paint",
    "b_ped": "paint",
    "gamma": "gamma",
    "agc_max": "agc_max",
    "total_dtl": "total_dtl",
    "genlock": "genlock",
    "monitor": "monitor",
    "lens_h": "lens_h",
}


def call(host: str, path: str, **params: Any) -> dict:
    q = urllib.parse.urlencode({k: v for k, v in params.items() if v is not None and v != ""})
    url = f"http://{host}/api/{path}?{q}" if q else f"http://{host}/api/{path}"
    req = urllib.request.Request(url, headers={"Cache-Control": "no-store"})
    with urllib.request.urlopen(req, timeout=TIMEOUT_S) as r:
        return json.loads(r.read().decode())


@dataclass
class Case:
    name: str
    path: str
    params: dict
    kind: str = "any"  # inquiry | set | move | danger
    expect_ok: Optional[bool] = None
    cap: Optional[str] = field(default=None)  # override


def all_cases(*, live: bool, safe: bool) -> list[Case]:
    c: list[Case] = [
        Case("status", "status", {}, kind="inquiry"),
        Case("preset_latest", "ptz", {"cmd": "preset_latest"}, "inquiry"),
        Case("pos", "ptz", {"cmd": "pos"}, "inquiry"),
        Case("pos_disp", "ptz", {"cmd": "pos_disp"}, "inquiry"),
        Case("req_zoom", "ptz", {"cmd": "req_zoom"}, "inquiry"),
        Case("req_focus", "ptz", {"cmd": "req_focus"}, "inquiry"),
        Case("req_iris", "ptz", {"cmd": "req_iris"}, "inquiry"),
        Case("req_lens", "ptz", {"cmd": "req_lens"}, "inquiry"),
        Case("req_exposure", "ptz", {"cmd": "req_exposure"}, "inquiry"),
        Case("req_error", "ptz", {"cmd": "req_error"}, "inquiry"),
        Case("req_landing", "ptz", {"cmd": "req_landing"}, "inquiry"),
        Case("req_ver", "ptz", {"cmd": "req_ver"}, "inquiry"),
        Case("req_heater", "ptz", {"cmd": "req_heater"}, "inquiry"),
        Case("req_defrost", "ptz", {"cmd": "req_defrost"}, "inquiry"),
        Case("preset_map", "ptz", {"cmd": "preset_map"}, "inquiry"),
        Case("raw #S", "ptz", {"cmd": "raw", "data": "S"}, "inquiry"),
        Case("raw #O", "ptz", {"cmd": "raw", "data": "O"}, "inquiry"),
        Case("cam model", "cam", {"cmd": "model"}, "inquiry"),
        Case("cam version", "cam", {"cmd": "version"}, "inquiry"),
        Case("cam params", "cam", {"cmd": "params"}, "inquiry"),
    ]

    if safe:
        return c

    c += [
        Case("preset_mode", "ptz", {"cmd": "preset_mode", "v": "0"}, "set"),
        Case("preset_speed", "ptz", {"cmd": "preset_speed", "v": "1"}, "set"),
        Case("preset_speed_unit", "ptz", {"cmd": "preset_speed_unit", "data": "080"}, "set"),
        Case("lens_ctrl", "ptz", {"cmd": "lens_ctrl", "v": "0"}, "set"),
        Case("tally", "ptz", {"cmd": "tally", "v": "0"}, "set"),
        Case("wireless", "ptz", {"cmd": "wireless", "v": "0"}, "set"),
        Case("washer", "ptz", {"cmd": "washer", "v": "0"}, "set"),
        Case("option", "ptz", {"cmd": "option", "v": "0"}, "set"),
        Case("fan", "ptz", {"cmd": "fan", "v": "0"}, "set"),
        Case("fan2", "ptz", {"cmd": "fan2", "v": "0"}, "set"),
        Case("wiper", "ptz", {"cmd": "wiper", "v": "0"}, "set"),
        Case("tilt_range", "ptz", {"cmd": "tilt_range", "v": "0"}, "set"),
        Case("set_zoom", "ptz", {"cmd": "set_zoom", "v": "0"}, "set"),
        Case("set_focus", "ptz", {"cmd": "set_focus", "v": "0"}, "set"),
        Case("set_iris", "ptz", {"cmd": "set_iris", "v": "32768"}, "set"),
        Case("set_lens", "ptz", {"cmd": "set_lens", "v3": "0", "data": "0", "v4": "32768"}, "set"),
        Case("set_pt", "ptz", {"cmd": "set_pt", "v": "0x8000", "v2": "0x8000"}, "set"),
        Case("limit_ctrl", "ptz", {"cmd": "limit_ctrl", "v": "1", "v2": "0"}, "set"),
        Case("preset_recall", "ptz", {"cmd": "preset_recall", "preset": "1"}, "set"),
        Case("home", "ptz", {"cmd": "home"}, "set"),
        Case("stop", "ptz", {"cmd": "stop"}, "set"),
        Case("boot", "ptz", {"cmd": "boot"}, "set"),
        Case("awc_mode", "cam", {"cmd": "awc_mode", "v": "0"}, "set"),
        Case("scene", "cam", {"cmd": "scene", "v": "0"}, "set"),
        Case("shutter_mode", "cam", {"cmd": "shutter_mode", "v": "0"}, "set"),
        Case("synchro", "cam", {"cmd": "synchro", "data": "1FF"}, "set"),
        Case("gain", "cam", {"cmd": "gain", "v": "0"}, "set"),
        Case("agc", "cam", {"cmd": "agc"}, "set"),
        Case("detail", "cam", {"cmd": "detail", "v": "0"}, "set"),
        Case("hd_detail", "cam", {"cmd": "hd_detail", "v": "0"}, "set"),
        Case("iris_am", "cam", {"cmd": "iris_am", "v": "1"}, "set"),
        Case("man_iris", "cam", {"cmd": "man_iris", "v": "32768"}, "set"),
        Case("filter", "cam", {"cmd": "filter", "v": "0"}, "set"),
        Case("colorbar off", "cam", {"cmd": "colorbar", "v": "0"}, "set"),
        Case("menu off", "cam", {"cmd": "menu", "v": "0"}, "set"),
        Case("af", "cam", {"cmd": "af", "v": "0"}, "set"),
        Case("negpos", "cam", {"cmd": "negpos", "v": "0"}, "set"),
        Case("fieldframe", "cam", {"cmd": "fieldframe", "v": "0"}, "set"),
        Case("chroma", "cam", {"cmd": "chroma", "v": "5"}, "set"),
        Case("black_shade", "cam", {"cmd": "black_shade", "v": "0"}, "set"),
        Case("r_gain", "cam", {"cmd": "r_gain", "data": "1E"}, "set"),
        Case("b_gain", "cam", {"cmd": "b_gain", "data": "1E"}, "set"),
        Case("gamma", "cam", {"cmd": "gamma", "data": "0"}, "set"),
        Case("monitor", "cam", {"cmd": "monitor", "data": "00", "data2": "0"}, "set"),
        Case("lens_h", "cam", {"cmd": "lens_h", "data": "00"}, "set"),
        Case("awc_start", "cam", {"cmd": "awc_start"}, "set"),
        Case("abc_start", "cam", {"cmd": "abc_start"}, "set"),
    ]

    if live:
        c += [
            Case("pan_left", "ptz", {"cmd": "pan_left", "rate": "15"}, "move"),
            Case("stop after pan", "ptz", {"cmd": "stop"}, "move"),
            Case("pan_right", "ptz", {"cmd": "pan_right", "rate": "15"}, "move"),
            Case("stop after pan2", "ptz", {"cmd": "stop"}, "move"),
            Case("tilt_up", "ptz", {"cmd": "tilt_up", "rate": "15"}, "move"),
            Case("stop after tilt", "ptz", {"cmd": "stop"}, "move"),
            Case("tilt_down", "ptz", {"cmd": "tilt_down", "rate": "15"}, "move"),
            Case("stop after tilt2", "ptz", {"cmd": "stop"}, "move"),
            Case("zoom_tele", "ptz", {"cmd": "zoom_tele", "rate": "15"}, "move"),
            Case("stop after zoom", "ptz", {"cmd": "stop"}, "move"),
            Case("zoom_wide", "ptz", {"cmd": "zoom_wide", "rate": "15"}, "move"),
            Case("stop after zoom2", "ptz", {"cmd": "stop"}, "move"),
            Case("focus_far", "ptz", {"cmd": "focus_far", "rate": "15"}, "move"),
            Case("stop after focus", "ptz", {"cmd": "stop"}, "move"),
            Case("focus_near", "ptz", {"cmd": "focus_near", "rate": "15"}, "move"),
            Case("stop after focus2", "ptz", {"cmd": "stop"}, "move"),
            Case("limit_toggle", "ptz", {"cmd": "limit_toggle", "v": "1"}, "move"),
            Case("power_on_cam", "ptz", {"cmd": "power_on_cam"}, "set"),
        ]

    return c


def cap_for(case: Case) -> Optional[str]:
    if case.cap is not None:
        return case.cap
    cmd = case.params.get("cmd", "")
    if case.path == "ptz":
        return PT_CAP.get(cmd)
    if case.path == "cam":
        return CAM_CAP.get(cmd)
    return None


def allowed(case: Case, status: dict) -> bool:
    key = cap_for(case)
    if key is None:
        return True
    caps = {}
    caps.update(status.get("pt_caps") or {})
    caps.update(status.get("cam_caps") or {})
    return bool(caps.get(key, False))


def classify(case: Case, d: dict) -> str:
    if d.get("status") == "exc":
        return "EXC"
    ok = bool(d.get("ok"))
    if case.path == "status":
        return "OK" if d.get("link") and d.get("model") else "FAIL"
    if case.expect_ok is True:
        return "OK" if ok else "FAIL"
    if case.expect_ok is False:
        return "OK" if not ok else "FAIL"
    if ok:
        return "OK"
    st = d.get("status", "")
    if st in ("timeout", "io", "nack", "param"):
        return "N/A"
    return "FAIL"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default=DEFAULT_HOST)
    ap.add_argument("--safe", action="store_true", help="только inquiry")
    ap.add_argument("--no-live", action="store_true", help="без move hold")
    args = ap.parse_args()
    host = args.host
    live = not args.no_live and not args.safe

    try:
        status = call(host, "status")
    except Exception as e:  # noqa: BLE001
        print(f"status failed: {e}", file=sys.stderr)
        return 2

    cases = [c for c in all_cases(live=live, safe=args.safe) if allowed(c, status)]
    skipped = len(all_cases(live=live, safe=args.safe)) - len(cases)

    counts = {"OK": 0, "N/A": 0, "FAIL": 0, "EXC": 0, "SKIP": skipped}
    print(
        f"host={host} pt={status.get('pt_id')} cam={status.get('cam_id')} "
        f"cases={len(cases)} skipped_caps={skipped} live={live} safe={args.safe}\n"
    )

    prev_kind = ""
    for case in cases:
        if case.kind != prev_kind:
            print(f"=== {case.kind} ===")
            prev_kind = case.kind
        try:
            if case.path == "status":
                d = call(host, "status")
            else:
                d = call(host, case.path, **case.params)
        except Exception as e:  # noqa: BLE001
            d = {"ok": False, "status": "exc", "error": str(e)}

        tag = classify(case, d)
        counts[tag] += 1
        show = {k: d[k] for k in list(d)[:6]}
        print(f"  {tag:4}  {case.name}: {show}")

        if case.kind == "move" and case.params.get("cmd") in (
            "pan_left",
            "pan_right",
            "tilt_up",
            "tilt_down",
            "zoom_tele",
            "zoom_wide",
            "focus_far",
            "focus_near",
        ):
            time.sleep(0.35)

    print(
        f"\n=== summary ===\n"
        f"  OK={counts['OK']}  N/A={counts['N/A']}  FAIL={counts['FAIL']}  "
        f"EXC={counts['EXC']}  SKIP={counts['SKIP']}  ran={sum(v for k,v in counts.items() if k!='SKIP')}"
    )
    return 1 if counts["FAIL"] or counts["EXC"] else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except urllib.error.URLError as e:
        print(f"host unreachable: {e}", file=sys.stderr)
        sys.exit(2)
