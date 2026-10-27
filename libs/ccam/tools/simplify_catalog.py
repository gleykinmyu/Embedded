#!/usr/bin/env python3
"""One-shot: strip unused name fields from camera/menu catalogs."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "include" / "catalog"


def simplify_camera() -> None:
    path = ROOT / "camera_commands.hpp"
    text = path.read_text(encoding="utf-8")
    text = text.replace(
        """struct CameraCommandEntry {
    const char* name;
    const char* query;
    const char* control;
    CameraPattern pattern;
};""",
        """struct CameraCommandEntry {
    const char* query;
    const char* control;
    CameraPattern pattern;
};""",
    )
    # {"Name", "Qxx", "Oxx", Pattern} -> {"Qxx", "Oxx", Pattern}
    text = re.sub(
        r'\{\s*"[^"]*"\s*,\s*("[^"]*"\s*,\s*"[^"]*"\s*,\s*CameraPattern::\w+)\s*\}',
        r"{\1}",
        text,
    )
    path.write_text(text, encoding="utf-8", newline="\n")
    print("updated", path)


def simplify_menu_file(path: Path) -> None:
    text = path.read_text(encoding="utf-8")
    # {0x00, "Gamma"}, -> 0x00, // Gamma
    text = re.sub(
        r'\{\s*(0x[0-9A-Fa-f]+|\d+)\s*,\s*"([^"]*)"\s*\}',
        r"\1,  // \2",
        text,
    )
    path.write_text(text, encoding="utf-8", newline="\n")
    print("updated", path)


def simplify_menu_entry() -> None:
    path = ROOT / "menu_entry.hpp"
    path.write_text(
        """/**
 * @file menu_entry.hpp
 * @brief Subcode меню Convertible Protocol (OSD / Video / Extended J).
 */

#pragma once

#include <cstdint>

namespace ccam::catalog {

using MenuSubcode = uint8_t;

} // namespace ccam::catalog
""",
        encoding="utf-8",
        newline="\n",
    )
    print("updated", path)


def simplify_menu_catalog() -> None:
    path = ROOT / "menu_catalog.hpp"
    path.write_text(
        """/**
 * @file menu_catalog.hpp
 * @brief Доступ к subcode-меню по enum (индекс в таблицу).
 */

#pragma once

#include "catalog/command_ids.hpp"
#include "catalog/extended_j_menu.hpp"
#include "catalog/osd_menu.hpp"
#include "catalog/video_menu.hpp"

#include <cstddef>
#include <cstdint>

namespace ccam::catalog {

inline constexpr uint8_t subcode(OsdItem item)
{
    return kOsdMenu[static_cast<size_t>(item)];
}

inline constexpr uint8_t subcode(VideoMenuItem item)
{
    return kVideoMenu[static_cast<size_t>(item)];
}

inline constexpr uint8_t subcode(ExtendedJItem item)
{
    return kExtendedJMenu[static_cast<size_t>(item)];
}

} // namespace ccam::catalog
""",
        encoding="utf-8",
        newline="\n",
    )
    print("updated", path)


def fix_menu_array_type(path: Path, array_name: str) -> None:
    text = path.read_text(encoding="utf-8")
    text = text.replace(
        f"inline constexpr MenuSubcodeEntry {array_name}[]",
        f"inline constexpr uint8_t {array_name}[]",
    )
    # count helpers if they use sizeof / MenuSubcodeEntry
    text = text.replace("MenuSubcodeEntry", "uint8_t")
    path.write_text(text, encoding="utf-8", newline="\n")
    print("typed", path)


def main() -> None:
    simplify_camera()
    for name in ("osd_menu.hpp", "video_menu.hpp", "extended_j_menu.hpp"):
        p = ROOT / name
        simplify_menu_file(p)
        array = {
            "osd_menu.hpp": "kOsdMenu",
            "video_menu.hpp": "kVideoMenu",
            "extended_j_menu.hpp": "kExtendedJMenu",
        }[name]
        fix_menu_array_type(p, array)
    simplify_menu_entry()
    simplify_menu_catalog()


if __name__ == "__main__":
    main()
