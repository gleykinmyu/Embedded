# PlatformIO script: generate nexHmiConfig.hpp from a Nextion .HMI file.
#
# Hooked from Nextion/library.json:
#   "build": { "extraScript": "syncHMI/pio_hmi_gen.py" }
#
# Project platformio.ini:
#   custom_hmi_gen  = off | onchange | always   (also: 0 | 1 | 2)
#   custom_hmi_file = src/MyPanel.HMI
#   custom_hmi_out  = src/UI/nexHmiConfig.hpp   (optional)
#
# Default custom_hmi_gen=off — no-op unless the project opts in.
# Import("env") is provided by PlatformIO.

Import("env")

import subprocess
import sys
from pathlib import Path

_env = env  # PlatformIO injects `env`


def _normalize_mode(raw):
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
            "pio_hmi_gen: unknown custom_hmi_gen=%r; "
            "use off | onchange | always (or 0 | 1 | 2)\n" % (raw,)
        )
        _env.Exit(1)
    return aliases[key]


def _needs_regen(hmi, out):
    if not out.is_file():
        return True
    return hmi.stat().st_mtime > out.stat().st_mtime


def _find_hmi2config(project_dir):
    """Locate hmi2config.py (SCons may not define __file__)."""
    candidates = []

    try:
        candidates.append(Path(__file__).resolve().parent / "hmi2config.py")
    except NameError:
        pass

    candidates.append(
        Path(project_dir) / "../../libs/Nextion/syncHMI/hmi2config.py"
    )
    candidates.append(Path(project_dir) / "lib/Nextion/syncHMI/hmi2config.py")

    extra_dirs = _env.GetProjectOption("lib_extra_dirs", default="")
    for line in str(extra_dirs).replace(",", "\n").splitlines():
        d = line.strip()
        if not d:
            continue
        base = Path(d) if Path(d).is_absolute() else Path(project_dir) / d
        candidates.append(base / "Nextion" / "syncHMI" / "hmi2config.py")

    for c in candidates:
        try:
            resolved = c.resolve()
        except OSError:
            continue
        if resolved.is_file():
            return resolved

    return candidates[0].resolve() if candidates else Path("hmi2config.py")


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


def main():
    if _env.IsCleanTarget():
        return

    mode = _normalize_mode(_env.GetProjectOption("custom_hmi_gen", default="off"))
    if mode == "off":
        return

    project_dir = Path(_env["PROJECT_DIR"]).resolve()
    hmi2config = _find_hmi2config(project_dir)

    hmi_rel = _env.GetProjectOption("custom_hmi_file", default="").strip()
    if not hmi_rel:
        sys.stderr.write(
            "pio_hmi_gen: custom_hmi_gen is enabled but custom_hmi_file is empty\n"
        )
        _env.Exit(1)

    out_rel = _env.GetProjectOption(
        "custom_hmi_out", default="src/UI/nexHmiConfig.hpp"
    ).strip()

    hmi_path = Path(hmi_rel)
    if not hmi_path.is_absolute():
        hmi_path = project_dir / hmi_path
    hmi_path = hmi_path.resolve()

    out_path = Path(out_rel)
    if not out_path.is_absolute():
        out_path = project_dir / out_path
    out_path = out_path.resolve()

    if not hmi_path.is_file():
        sys.stderr.write("pio_hmi_gen: HMI not found: %s\n" % hmi_path)
        _env.Exit(1)
    if not hmi2config.is_file():
        sys.stderr.write("pio_hmi_gen: generator not found: %s\n" % hmi2config)
        _env.Exit(1)

    if mode == "onchange" and not _needs_regen(hmi_path, out_path):
        print("pio_hmi_gen: up to date (%s)" % out_path.name)
        return

    _run_hmi2config(hmi_path, out_path, hmi2config, project_dir)


main()
