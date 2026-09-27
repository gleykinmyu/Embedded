# PlatformIO pre-script: write compile_commands.json on a normal build.
# `pio run -t compiledb` already creates the file. This script also turns the
# bare compiler name into the toolchain path on this machine, and copies the
# database into each framework package this project uses. Headers opened from
# ~/.platformio then pick up the project that was just built.
Import("env")

import json
from pathlib import Path

from SCons.Script import COMMAND_LINE_TARGETS, Default


def _absolute_tool(env, key):
    if key not in env:
        return
    current = str(env[key]).strip().strip('"')
    if not current or Path(current).is_absolute():
        return
    found = env.WhereIs(current) or env.WhereIs(current + ".exe")
    if found:
        env[key] = found


def _resolve_compiler(env, token):
    token = token.replace("\\", "/")
    if "/" in token:
        return token
    found = env.WhereIs(token) or env.WhereIs(token + ".exe")
    if found:
        return Path(found).as_posix()
    return token


def _framework_dirs(env):
    platform = env.PioPlatform()
    names = env.get("PIOFRAMEWORK") or []
    if isinstance(names, str):
        names = [part.strip() for part in names.split(",") if part.strip()]
    frameworks = getattr(platform, "frameworks", {}) or {}
    dirs = []
    for name in names:
        key = str(name).lower()
        package = (frameworks.get(key) or frameworks.get(name) or {}).get("package")
        if not package:
            continue
        path = platform.get_package_dir(package)
        if path:
            dirs.append(Path(path))
    return dirs


def _publish_framework_db(env, database):
    payload = database.read_bytes()
    for folder in _framework_dirs(env):
        dest = folder / "compile_commands.json"
        dest.write_bytes(payload)
        print(f"Framework headers: {dest}")


def _normalize(target, source, env):
    database = Path(env.subst("$COMPILATIONDB_PATH"))
    if not database.is_file():
        print(f"compile_commands.json was not written: {database}")
        return 1
    data = json.loads(database.read_text(encoding="utf-8"))
    for entry in data:
        for key in ("directory", "file", "output", "command"):
            value = entry.get(key)
            if isinstance(value, str):
                entry[key] = value.replace("\\", "/")
        command = entry.get("command")
        if isinstance(command, str):
            token, sep, rest = command.partition(" ")
            entry["command"] = _resolve_compiler(env, token) + (sep + rest if sep else "")
    database.write_text(
        json.dumps(data, indent=4, ensure_ascii=False) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    _publish_framework_db(env, database)
    sample = next(
        (
            entry["command"].split(" ", 1)[0]
            for entry in data
            if str(entry.get("file", "")).endswith("main/main.c")
        ),
        "",
    )
    print(f"Updated {database.name}: {len(data)} entries, compiler {sample}")
    return 0


def _attach(nodes):
    env.AddPostAction(nodes, _normalize)
    return nodes


for _tool in ("CC", "CXX", "AS"):
    _absolute_tool(env, _tool)

if "compiledb" in COMMAND_LINE_TARGETS:
    _original = env.CompilationDatabase

    # AddMethod passes the environment as the first argument.
    def _wrapped(caller_env, target=None, *args, **kwargs):
        return _attach(_original(target, *args, **kwargs))

    env.AddMethod(_wrapped, "CompilationDatabase")
else:
    env.Tool("compilation_db")
    database = env.subst("$COMPILATIONDB_PATH")
    Default(_attach(env.CompilationDatabase(database)))
