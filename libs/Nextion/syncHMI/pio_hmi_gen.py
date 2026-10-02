# PlatformIO script: generate nexHmiConfig.hpp from a Nextion .HMI file.
#
# Hooked from Nextion/library.json:
#   "build": { "extraScript": "syncHMI/pio_hmi_gen.py" }
#
# Project platformio.ini:
#   custom_hmi_gen  = off | onchange | always   (also: 0 | 1 | 2)
#   custom_hmi_file = src/MyPanel.HMI
#   custom_hmi_out  = src/UI/nexHmiConfig.hpp   (optional)
#   custom_font_gen = off | onchange | always   (also: 0 | 1 | 2)
#
# custom_font_gen uses custom_hmi_file and appends fonts to custom_hmi_out.
# Both flags default to off — no-op unless the project opts in.
# Import("env") is provided by PlatformIO.

Import("env")

import subprocess
import sys
from pathlib import Path

_env = env  # PlatformIO injects `env`


def _normalize_mode(raw, option):
    key = raw.strip().lower()
    aliases = {
        "0": "off",
        "off": "off",
        "none": "off",
        "no": "off",
        "false": "off",
        "1": "onchange",
        "onchange": "onchange",
        "change": "onchange",
        "ifchanged": "onchange",
        "2": "always",
        "always": "always",
        "force": "always",
        "yes": "always",
        "true": "always",
    }
    if key not in aliases:
        sys.stderr.write(
            "pio_hmi_gen: unknown %s=%r; "
            "use off | onchange | always (or 0 | 1 | 2)\n" % (option, raw)
        )
        _env.Exit(1)
    return aliases[key]


def _needs_regen(hmi, out):
    if not out.is_file():
        return True
    return hmi.stat().st_mtime > out.stat().st_mtime


def _option_entries(raw):
    """Project options may be a string, a list, or a comma-separated list."""
    if raw is None:
        return []
    if isinstance(raw, (list, tuple)):
        items = raw
    else:
        items = str(raw).replace(",", "\n").splitlines()
    entries = []
    for item in items:
        text = str(item).strip().strip("'\"")
        if text:
            entries.append(text)
    return entries


def _find_sync_script(project_dir, script_name):
    """Locate a syncHMI script (SCons may not define __file__)."""
    candidates = []

    try:
        candidates.append(Path(__file__).resolve().parent / script_name)
    except NameError:
        pass

    # DMX is Embedded/DMX (../libs). PUMS/Console is one level deeper (../../libs).
    project = Path(project_dir).resolve()
    here = project
    for _ in range(4):
        candidates.append(here / "libs" / "Nextion" / "syncHMI" / script_name)
        parent = here.parent
        if parent == here:
            break
        here = parent

    candidates.append(project / "lib" / "Nextion" / "syncHMI" / script_name)

    for entry in _option_entries(_env.GetProjectOption("lib_extra_dirs", default="")):
        base = Path(entry)
        if not base.is_absolute():
            base = project / base
        candidates.append(base / "Nextion" / "syncHMI" / script_name)

    seen = set()
    for c in candidates:
        try:
            resolved = c.resolve()
        except OSError:
            continue
        if resolved in seen:
            continue
        seen.add(resolved)
        if resolved.is_file():
            return resolved

    tried = "\n".join("  %s" % p for p in seen) or "  (none)"
    sys.stderr.write(
        "pio_hmi_gen: %s not found. Tried:\n%s\n" % (script_name, tried)
    )
    return None


def _project_path(project_dir, rel):
    path = Path(rel)
    if not path.is_absolute():
        path = project_dir / path
    return path.resolve()


def _run_hmi2config(hmi, out, script, project_dir):
    cmd = [
        sys.executable,
        str(script),
        "--update",
        "-i",
        str(hmi),
        "-o",
        str(out),
    ]
    print("pio_hmi_gen: %s" % " ".join(cmd))
    result = subprocess.run(cmd, cwd=str(project_dir))
    if result.returncode != 0:
        sys.stderr.write("pio_hmi_gen: hmi2config.py failed\n")
        _env.Exit(result.returncode)


def _require_hmi(project_dir, option):
    hmi_rel = _env.GetProjectOption("custom_hmi_file", default="").strip()
    if not hmi_rel:
        sys.stderr.write(
            "pio_hmi_gen: %s is enabled but custom_hmi_file is empty\n" % (option,)
        )
        _env.Exit(1)
    hmi_path = _project_path(project_dir, hmi_rel)
    if not hmi_path.is_file():
        sys.stderr.write("pio_hmi_gen: HMI not found: %s\n" % hmi_path)
        _env.Exit(1)
    return hmi_path


def _gen_widgets(project_dir):
    mode = _normalize_mode(
        _env.GetProjectOption("custom_hmi_gen", default="off"),
        "custom_hmi_gen",
    )
    if mode == "off":
        return

    hmi_path = _require_hmi(project_dir, "custom_hmi_gen")
    out_path = _project_path(
        project_dir,
        _env.GetProjectOption("custom_hmi_out", default="src/UI/nexHmiConfig.hpp").strip(),
    )
    script = _find_sync_script(project_dir, "hmi2config.py")
    if script is None:
        _env.Exit(1)

    if mode == "onchange" and not _needs_regen(hmi_path, out_path):
        print("pio_hmi_gen: up to date (%s)" % out_path.name)
        return

    _run_hmi2config(hmi_path, out_path, script, project_dir)


def _run_hmi2font(hmi, out, script, project_dir):
    cmd = [
        sys.executable,
        str(script),
        "-i",
        str(hmi),
        "-o",
        str(out),
    ]
    print("pio_hmi_gen: %s" % " ".join(cmd))
    result = subprocess.run(cmd, cwd=str(project_dir))
    if result.returncode != 0:
        sys.stderr.write("pio_hmi_gen: hmi2font.py failed\n")
        _env.Exit(result.returncode)


def _gen_fonts(project_dir):
    mode = _normalize_mode(
        _env.GetProjectOption("custom_font_gen", default="off"),
        "custom_font_gen",
    )
    if mode == "off":
        return

    hmi_path = _require_hmi(project_dir, "custom_font_gen")
    out_path = _project_path(
        project_dir,
        _env.GetProjectOption("custom_hmi_out", default="src/UI/nexHmiConfig.hpp").strip(),
    )
    script = _find_sync_script(project_dir, "hmi2font.py")
    if script is None:
        _env.Exit(1)

    if mode == "onchange" and not _needs_regen(hmi_path, out_path):
        print("pio_hmi_gen: up to date (%s)" % out_path.name)
        return

    _run_hmi2font(hmi_path, out_path, script, project_dir)


def main():
    if _env.IsCleanTarget():
        return

    project_dir = Path(_env["PROJECT_DIR"]).resolve()
    _gen_widgets(project_dir)
    _gen_fonts(project_dir)


main()
