#!/usr/bin/env python3
"""
Generate / update C++ HMI config from a Nextion .HMI file.

Uses the binary parser from Nextion2Text.py (Max Zuidberg, MPL-2.0).

Output: enum class with panel component ids + name table indexed by id.
Blocks between GENERATED-HMI-BEGIN/END markers are replaced on --update.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

_SCRIPT_DIR = Path(__file__).resolve().parent
if str(_SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPT_DIR))

from Nextion2Text import Component, HMI  # noqa: E402

PAGE_COMPONENT_TYPE = 121
K_PAGE_COMP_ID = 0
K_FIRST_COMP_ID = 1

# Nextion keyboard overlay pages — not part of application UI.
_SERVICE_PAGE_NAMES = frozenset({"keybdA", "keybdB", "keybdC"})
_SERVICE_PAGE_CONSTANTS = {
    "keybdA": "kKeybdAPageId",
    "keybdB": "kKeybdBPageId",
    "keybdC": "kKeybdCPageId",
}

_CPP_KEYWORDS = {
    "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
    "bool", "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t",
    "class", "compl", "concept", "const", "consteval", "constexpr", "constinit",
    "const_cast", "continue", "co_await", "co_return", "co_yield", "decltype",
    "default", "delete", "do", "double", "dynamic_cast", "else", "enum",
    "explicit", "export", "extern", "false", "float", "for", "friend", "goto",
    "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept",
    "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected",
    "public", "register", "reinterpret_cast", "requires", "return", "short",
    "signed", "sizeof", "static", "static_assert", "static_cast", "struct",
    "switch", "template", "this", "thread_local", "throw", "true", "try",
    "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual",
    "void", "volatile", "wchar_t", "while", "xor", "xor_eq",
}

_BLOCK_RE = re.compile(
    r"// GENERATED-HMI-BEGIN:(?P<tag>[^\n\r]+)\r?\n.*?// GENERATED-HMI-END:(?P=tag)",
    re.DOTALL,
)


@dataclass(frozen=True)
class WidgetInfo:
    panel_id: int
    objname: str
    type_code: int
    type_name: str


@dataclass(frozen=True)
class PageInfo:
    panel_id: int
    objname: str
    widgets: Tuple[WidgetInfo, ...] = field(default_factory=tuple)


def find_hmi_near_main(src_dir: Path) -> Optional[Path]:
    """Pick the only *.HMI / *.hmi next to main*.cpp in src/."""
    candidates = sorted(
        list(src_dir.glob("*.HMI")) + list(src_dir.glob("*.hmi")),
        key=lambda p: p.name.lower(),
    )
    if len(candidates) == 1:
        return candidates[0]
    if not candidates:
        return None
    mains = sorted(src_dir.glob("main*.cpp"))
    if len(mains) == 1:
        stem = mains[0].stem
        for c in candidates:
            if c.stem.lower() == stem.lower():
                return c
    return candidates[0]


def _cpp_string_literal(value: str) -> str:
    """Escape a Python string for use inside a C++ double-quoted literal."""
    escaped = (
        value.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\r", "\\r")
        .replace("\n", "\\n")
        .replace("\t", "\\t")
    )
    return f'"{escaped}"'


def _type_name(type_code: int) -> str:
    mapping = Component.attributes["type"]["mapping"]
    return str(mapping.get(type_code, f"Unknown({type_code})"))


def _sanitize_cpp_identifier(raw: str, *, fallback: str) -> str:
    out: List[str] = []
    for i, ch in enumerate(raw):
        if ch.isalnum() or ch == "_":
            out.append(ch)
        else:
            out.append("_")
    ident = "".join(out).strip("_")
    if not ident:
        ident = fallback
    if ident[0].isdigit():
        ident = "c_" + ident
    if ident in _CPP_KEYWORDS:
        ident = ident + "_"
    return ident


def _unique_enum_names(widgets: Sequence[WidgetInfo]) -> List[Tuple[str, WidgetInfo]]:
    used: Dict[str, int] = {}
    result: List[Tuple[str, WidgetInfo]] = []
    for w in widgets:
        base = _sanitize_cpp_identifier(w.objname, fallback=f"comp_{w.panel_id}")
        count = used.get(base, 0)
        used[base] = count + 1
        enum_name = base if count == 0 else f"{base}_{count + 1}"
        result.append((enum_name, w))
    return result


def _page_objname(page) -> str:
    name = page.header.name or "page"
    for comp in page.components:
        att = comp.rawData.get("att", {})
        if int(att.get("type", -1)) == PAGE_COMPONENT_TYPE and att.get("objname"):
            name = str(att["objname"])
    return name


def extract_pages(hmi: HMI) -> List[PageInfo]:
    pages: List[PageInfo] = []
    for page_index, page in enumerate(hmi.pages):
        # In .pa blobs the Page object (type 121) always has id=0; route.page uses project order.
        page_panel_id = page_index
        page_objname = _page_objname(page)
        widgets: List[WidgetInfo] = []

        for comp in page.components:
            att = comp.rawData.get("att", {})
            type_code = int(att.get("type", -1))
            panel_id = int(att.get("id", 0))
            objname = str(att.get("objname", ""))

            if type_code == PAGE_COMPONENT_TYPE:
                continue

            if panel_id < K_FIRST_COMP_ID:
                continue

            widgets.append(
                WidgetInfo(
                    panel_id=panel_id,
                    objname=objname,
                    type_code=type_code,
                    type_name=_type_name(type_code),
                )
            )

        widgets.sort(key=lambda w: w.panel_id)
        if page_objname in _SERVICE_PAGE_NAMES:
            continue
        pages.append(
            PageInfo(
                panel_id=page_panel_id,
                objname=page_objname,
                widgets=tuple(widgets),
            )
        )

    pages.sort(key=lambda p: p.panel_id)
    return pages


def extract_service_pages(hmi: HMI) -> List[Tuple[int, str]]:
    service_pages: List[Tuple[int, str]] = []
    for page_index, page in enumerate(hmi.pages):
        page_objname = _page_objname(page)
        if page_objname in _SERVICE_PAGE_NAMES:
            service_pages.append((page_index, page_objname))
    return service_pages


def _page_struct_name(page: PageInfo) -> str:
    return "Page_" + _sanitize_cpp_identifier(page.objname, fallback=f"id{page.panel_id}")


def _page_xmacro_name(page: PageInfo) -> str:
    return "HMI_PAGE_" + _sanitize_cpp_identifier(page.objname, fallback=f"id{page.panel_id}")


def _render_component_xmacro(page: PageInfo, macro_name: str) -> str:
    enum_pairs = _unique_enum_names(page.widgets)
    page_sym = _sanitize_cpp_identifier(page.objname, fallback=f"page_{page.panel_id}")
    entries: List[str] = [
        f"    X({page_sym}, {K_PAGE_COMP_ID}, ##__VA_ARGS__) \\"
    ]
    for enum_name, widget in enum_pairs:
        entries.append(f"    X({enum_name}, {widget.panel_id}, ##__VA_ARGS__) \\")
    if entries:
        entries[-1] = entries[-1].rstrip(" \\")

    lines = [f"#define {macro_name}(X, ...) \\"]
    lines.extend(entries)
    return "\n".join(lines)


def _render_names_array(
    page: PageInfo,
    macro_name: str,
    names_by_id: Dict[int, str],
    names_size: int,
) -> List[str]:
    dense = all(i in names_by_id for i in range(names_size))
    lines = ["    static constexpr const char* kNames[] = {"]
    if dense:
        lines.append(f"        {macro_name}(HMI_NAME_ITEM)")
    else:
        for idx in range(names_size):
            name = names_by_id.get(idx)
            if name is None:
                lines.append("        nullptr,")
            else:
                lines.append(f"        {_cpp_string_literal(name)},")
    lines.append("    };")
    return lines


def _render_page_struct(page: PageInfo) -> str:
    struct_name = _page_struct_name(page)
    macro_name = _page_xmacro_name(page)
    max_id = max((w.panel_id for w in page.widgets), default=K_PAGE_COMP_ID)
    names_size = max_id + 1

    names_by_id: Dict[int, str] = {K_PAGE_COMP_ID: page.objname}
    for widget in page.widgets:
        names_by_id[widget.panel_id] = widget.objname

    lines: List[str] = []
    lines.append(_render_component_xmacro(page, macro_name))
    lines.append("")
    lines.append(f"/** Page \"{page.objname}\" (panel page id {page.panel_id}). */")
    lines.append(f"struct {struct_name} {{")
    lines.append(f"    static constexpr uint8_t kPageId = {page.panel_id}u;")
    lines.append("")
    lines.append("    enum class Id : uint8_t {")
    lines.append(f"        {macro_name}(HMI_ENUM_ITEM)")
    lines.append("    };")
    lines.append("")
    lines.extend(_render_names_array(page, macro_name, names_by_id, names_size))
    lines.append("};")
    return "\n".join(lines)


def _render_summary(hmi: HMI, pages: Sequence[PageInfo], service_pages: Sequence[Tuple[int, str]]) -> str:
    lines = [
        f"inline constexpr const char* kHmiSource = {_cpp_string_literal(hmi.modelName)};",
        f"inline constexpr const char* kHmiModel = {_cpp_string_literal(hmi.modelDesc)};",
        f"inline constexpr uint8_t kPageCount = {len(pages)}u;",
    ]
    for page_id, objname in service_pages:
        const_name = _SERVICE_PAGE_CONSTANTS.get(objname)
        if const_name:
            lines.append(f"inline constexpr uint8_t {const_name} = {page_id}u;")
    present = {name for _, name in service_pages}
    for objname in sorted(_SERVICE_PAGE_NAMES):
        flag = f"kHas{objname}Page"
        lines.append(f"inline constexpr bool {flag} = {'true' if objname in present else 'false'};")
    return "\n".join(lines)


def _render_pages_block(pages: Sequence[PageInfo]) -> str:
    if not pages:
        return "// No pages found in HMI."
    return "\n\n".join(_render_page_struct(p) for p in pages)


def _wrap_block(tag: str, body: str) -> str:
    return f"// GENERATED-HMI-BEGIN:{tag}\n{body}\n// GENERATED-HMI-END:{tag}"


def render_blocks(
    hmi: HMI,
    pages: Sequence[PageInfo],
    service_pages: Sequence[Tuple[int, str]],
) -> Dict[str, str]:
    return {
        "summary": _render_summary(hmi, pages, service_pages),
        "pages": _render_pages_block(pages),
    }


_DEPRECATED_BLOCKS = ("helpers",)


def _namespace_open(namespace: str) -> str:
    return "\n".join(f"namespace {part} {{" for part in namespace.split("::"))


def _namespace_close(namespace: str) -> str:
    return "\n".join(
        f"}} // namespace {part}" for part in reversed(namespace.split("::"))
    )


def render_full_header(
    *,
    hmi_path: Path,
    namespace: str,
    blocks: Dict[str, str],
) -> str:
    parts = [
        "#pragma once",
        f"// Generated by hmi2config.py from {hmi_path.name}",
        "// Re-sync: python ../../syncHMI/hmi2config.py --update",
        "",
        "#include \"syncHMI/nexHmiSync.hpp\"",
        "",
        _namespace_open(namespace),
        "",
        _wrap_block("summary", blocks["summary"]),
        "",
        _wrap_block("pages", blocks["pages"]),
        "",
        _namespace_close(namespace),
    ]
    return "\n".join(parts) + "\n"


def patch_existing(content: str, blocks: Dict[str, str]) -> Tuple[str, List[str]]:
    """Replace or append GENERATED-HMI blocks. Returns (new_content, warnings)."""
    warnings: List[str] = []
    updated = content

    for tag in _DEPRECATED_BLOCKS:
        if tag not in blocks:
            pattern = re.compile(
                rf"// GENERATED-HMI-BEGIN:{re.escape(tag)}\r?\n.*?// GENERATED-HMI-END:{re.escape(tag)}\r?\n?",
                re.DOTALL,
            )
            updated, n = pattern.subn("", updated)
            if n:
                warnings.append(f"Removed deprecated block '{tag}'.")

    for tag, body in blocks.items():
        new_block = _wrap_block(tag, body)
        pattern = re.compile(
            rf"// GENERATED-HMI-BEGIN:{re.escape(tag)}\r?\n.*?// GENERATED-HMI-END:{re.escape(tag)}",
            re.DOTALL,
        )
        if pattern.search(updated):
            updated = pattern.sub(new_block, updated, count=1)
        else:
            warnings.append(f"Block '{tag}' not found — appended before closing namespace.")
            close_idx = updated.rfind("}")
            if close_idx >= 0:
                insert = "\n\n" + new_block + "\n"
                updated = updated[:close_idx] + insert + updated[close_idx:]
            else:
                updated = updated.rstrip() + "\n\n" + new_block + "\n"

    return updated, warnings


def load_hmi(path: Path) -> HMI:
    return HMI(str(path))


def default_output_path(hmi_path: Path, src_dir: Path) -> Path:
    return src_dir / "nexHmiConfig.hpp"


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate C++ enum/id + name tables from a Nextion .HMI file.",
    )
    parser.add_argument(
        "-i", "--input",
        type=Path,
        help="Path to .HMI (default: single *.HMI in --src-dir)",
    )
    parser.add_argument(
        "-o", "--output",
        type=Path,
        help="Output .hpp (default: <src-dir>/nexHmiConfig.hpp)",
    )
    parser.add_argument(
        "--src-dir",
        type=Path,
        default=Path("src"),
        help="Directory with main*.cpp and optional .HMI (default: src)",
    )
    parser.add_argument(
        "-n", "--namespace",
        default="nex::hmi",
        help='C++ namespace (default: "nex::hmi")',
    )
    parser.add_argument(
        "--update",
        action="store_true",
        help="Patch GENERATED-HMI blocks in an existing output file",
    )
    parser.add_argument(
        "--stdout",
        action="store_true",
        help="Print result to stdout instead of writing a file",
    )
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    src_dir = args.src_dir.resolve()

    hmi_path = args.input
    if hmi_path is None:
        hmi_path = find_hmi_near_main(src_dir)
        if hmi_path is None:
            print(
                f"error: no .HMI found in {src_dir}. Use -i or place exactly one *.HMI next to main.cpp.",
                file=sys.stderr,
            )
            return 1
    hmi_path = hmi_path.resolve()
    if not hmi_path.is_file():
        print(f"error: HMI not found: {hmi_path}", file=sys.stderr)
        return 1

    output_path = args.output
    if output_path is None:
        output_path = default_output_path(hmi_path, src_dir)
    output_path = output_path.resolve()

    try:
        hmi = load_hmi(hmi_path)
    except Exception as exc:
        print(f"error: failed to parse HMI: {exc}", file=sys.stderr)
        return 1

    pages = extract_pages(hmi)
    service_pages = extract_service_pages(hmi)
    blocks = render_blocks(hmi, pages, service_pages)
    text: str
    warnings: List[str] = []

    if args.update and output_path.is_file():
        existing = output_path.read_text(encoding="utf-8")
        if "// GENERATED-HMI-BEGIN:" not in existing:
            print(
                f"warning: {output_path} has no GENERATED-HMI markers; writing a fresh file.",
                file=sys.stderr,
            )
            text = render_full_header(
                hmi_path=hmi_path, namespace=args.namespace, blocks=blocks
            )
        else:
            text, warnings = patch_existing(existing, blocks)
    else:
        text = render_full_header(hmi_path=hmi_path, namespace=args.namespace, blocks=blocks)

    if args.stdout:
        sys.stdout.write(text)
    else:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        with output_path.open("w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print(f"Wrote {output_path} ({len(pages)} page(s), source {hmi_path.name})")

    for w in warnings:
        print(f"warning: {w}", file=sys.stderr)

    pages_by_id = {p.panel_id: p for p in pages}
    for page_index, page in enumerate(hmi.pages):
        name = _page_objname(page)
        if name in _SERVICE_PAGE_NAMES:
            print(
                f"  page {page_index}: \"{name}\" — skipped (service page)",
                file=sys.stderr,
            )
            continue
        matched = pages_by_id.get(page_index)
        if matched:
            print(
                f"  page {matched.panel_id}: \"{matched.objname}\" — {len(matched.widgets)} widget(s)",
                file=sys.stderr,
            )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
