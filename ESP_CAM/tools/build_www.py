#!/usr/bin/env python3
"""
Сборка src/www/status.html из модулей + IPTV UI kit (inline для ESP).

  python tools/build_www.py
  python tools/build_www.py --out src/generated/status.html
"""

from __future__ import annotations

import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WWW = ROOT / "src" / "www"
KIT = WWW / "vendor" / "iptv-ui" / "kit"
OUT = WWW / "status.html"


def read(p: Path) -> str:
    return p.read_text(encoding="utf-8")


def strip_html_comments(s: str) -> str:
    # убрать HTML-комментарии, кроме условных IE (их нет)
    import re

    return re.sub(r"<!--.*?-->", "", s, flags=re.S)


def expand_includes(html: str) -> str:
    import re

    def repl(m: re.Match[str]) -> str:
        rel = m.group(1).strip()
        path = WWW / rel
        if not path.is_file():
            raise SystemExit(f"include missing: {rel}")
        body = read(path)
        # confirm.html содержит <script src=...> — выкидываем, JS инлайним отдельно
        body = re.sub(
            r'<script\s+src=["\'][^"\']+["\']\s*>\s*</script>',
            "",
            body,
            flags=re.I,
        )
        return body.strip() + "\n"

    return re.sub(r"<!--\s*INCLUDE\s+([^\s]+)\s*-->", repl, html)


def assemble() -> str:
    index = read(WWW / "index.html")
    html = expand_includes(index)

    css_parts = [
        "/* === IPTV kit: page.css === */",
        read(KIT / "css" / "page.css"),
        "/* === IPTV kit: modal.css === */",
        read(KIT / "css" / "modal.css"),
        "/* === ESP_CAM app.css === */",
        read(WWW / "css" / "app.css"),
        "/* === ESP_CAM osd.css === */",
        read(WWW / "css" / "osd.css"),
    ]
    js_parts = [
        "/* === IPTV kit: modal.js === */",
        read(KIT / "js" / "modal.js"),
        "/* === IPTV kit: note.js === */",
        read(KIT / "js" / "note.js"),
        "/* === IPTV kit: confirm.js === */",
        read(KIT / "js" / "confirm.js"),
        "/* === ESP_CAM app.js === */",
        read(WWW / "js" / "app.js"),
        "/* === ESP_CAM osd.js === */",
        read(WWW / "js" / "osd.js"),
    ]

    css = "\n".join(css_parts)
    js = "\n".join(js_parts)

    if "/*INLINE_CSS*/" not in html or "/*INLINE_JS*/" not in html:
        raise SystemExit("index.html must contain /*INLINE_CSS*/ and /*INLINE_JS*/ placeholders")

    html = html.replace("/*INLINE_CSS*/", css)
    html = html.replace("/*INLINE_JS*/", js)
    return html


def main() -> None:
    ap = argparse.ArgumentParser(description="Assemble ESP_CAM www → status.html")
    ap.add_argument(
        "--out",
        type=Path,
        help="Extra output path (e.g. src/generated/status.html for EMBED_TXTFILES)",
    )
    args = ap.parse_args()

    html = assemble()
    OUT.write_text(html, encoding="utf-8", newline="\n")
    has_tabs = 'class="tab' in html
    print(f"wrote {OUT.relative_to(ROOT)} ({len(html)} bytes)")
    print(f"  kit={KIT.exists()} tabs={has_tabs}")
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(html, encoding="utf-8", newline="\n")
        print(f"wrote {args.out} ({args.out.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
