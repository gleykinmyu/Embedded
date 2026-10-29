from pathlib import Path

src = Path(__file__).resolve().parents[1] / "src" / "www" / "status.html"
body = "R\"html(" + src.read_text(encoding="utf-8") + ")html\"\n"
root = Path(__file__).resolve().parents[2] / ".pio_core_root" / "build" / "ESP_CAM"
incs = list(root.rglob("status.html.inc")) if root.exists() else []
print(f"src len={len(body)} ptz={'data-tab=\"ptz\"' in body} old={'Head PT' in body}")
print(f"inc files: {len(incs)}")
for p in incs:
    p.write_text(body, encoding="utf-8")
    got = p.read_text(encoding="utf-8")
    print(f"  {p}: ptz={'data-tab=\"ptz\"' in got} old={'Head PT' in got}")
