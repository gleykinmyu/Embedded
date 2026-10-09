# Compile official LVGL demos from the libdeps package (widgets, etc.).
Import("env")
from pathlib import Path

libdeps = Path(env["PROJECT_LIBDEPS_DIR"]) / env["PIOENV"] / "lvgl" / "demos"
if libdeps.is_dir():
    env.BuildSources(
        str(Path(env["PROJECT_BUILD_DIR"]) / env["PIOENV"] / "lvgl_demos"),
        str(libdeps),
        "+<*>",
    )
    print(f"[DMX_LVGL] Including LVGL demos from {libdeps}")
else:
    print(f"[DMX_LVGL] WARNING: LVGL demos not found at {libdeps}")
