"""Build gui/res/app.ico from gui/assets/app.png.

Pillow writes a multi-resolution Windows .ico (16/24/32/48/64/128/256) so
Explorer, the taskbar, Alt+Tab, and tiny tray contexts all pick the right
size. The source PNG can have alpha; it is preserved.

Usage:
    python scripts/make_icon.py
"""
from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "gui" / "assets" / "app.png"
DST = ROOT / "gui" / "res" / "app.ico"
SIZES = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]


def main() -> int:
    if not SRC.exists():
        print(f"[make_icon] missing source: {SRC}", file=sys.stderr)
        return 1

    im = Image.open(SRC).convert("RGBA")
    DST.parent.mkdir(parents=True, exist_ok=True)
    im.save(DST, format="ICO", sizes=SIZES)
    print(f"[make_icon] wrote {DST} ({im.size[0]}x{im.size[1]} source, "
          f"{len(SIZES)} sizes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
