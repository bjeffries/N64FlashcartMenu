#!/usr/bin/env python3
"""Generate the pixel-art N64 cartridges drawn over every Library label.

Writes two sprites (RGBA, transparent background) into assets/images/:
  cartridge.png        unselected tiles
  cartridge_large.png  the selected tile, ~30% larger

Each sprite is an overlay: the label is drawn first, then the cartridge on top,
and the label shows through a transparent window with rounded corners and a
dark rim. The printed window positions must match CARTRIDGE_* and
CARTRIDGE_LARGE_* in src/menu/ui_components/constants.h.

Usage: scripts/make_cartridge.py [out_dir=assets/images]
"""

import os
import sys
from PIL import Image

BODY = (0xC4, 0xC4, 0xC4, 0xFF)
HIGHLIGHT = (0xD8, 0xD8, 0xD8, 0xFF)
SEAM = (0x8A, 0x8A, 0x8A, 0xFF)
OUTLINE = (0x9A, 0x9A, 0x9A, 0xFF)
RECESS = (0x6A, 0x6A, 0x6A, 0xFF)
CLEAR = (0, 0, 0, 0)

# Base (unselected) geometry; the large sprite scales these.
BASE_W, BASE_H = 88, 62
BASE_WING = 15
BASE_LABEL_Y, BASE_LABEL_W, BASE_LABEL_H = 6, 44, 51
BASE_RADIUS = 4


def in_rounded_rect(x, y, rx, ry, rw, rh, radius):
    """True if pixel (x, y) is inside the rounded rectangle, testing the pixel centre."""
    cx = min(max(x + 0.5, rx + radius), rx + rw - radius)
    cy = min(max(y + 0.5, ry + radius), ry + rh - radius)
    return (x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 <= radius ** 2


def build(scale, out_path):
    s = lambda v: round(v * scale)
    w, h, wing = s(BASE_W), s(BASE_H), s(BASE_WING)
    panel_x0, panel_x1 = wing + 1, w - wing - 2          # centre panel columns (inclusive)
    label_w, label_h, label_y = s(BASE_LABEL_W), s(BASE_LABEL_H), s(BASE_LABEL_Y)
    label_x = panel_x0 + ((panel_x1 - panel_x0 + 1) - label_w) // 2
    radius = s(BASE_RADIUS)
    arch, slope = 2.4 * scale, s(4)
    groove_y, groove_w = h - s(15), s(5)

    def top_edge(x):
        """First opaque row of column x: an arch over the panel, lower sloping wings."""
        if panel_x0 - 1 <= x <= panel_x1 + 1:
            centre = (panel_x0 + panel_x1) / 2
            half = (panel_x1 - panel_x0) / 2
            return round(arch * ((x - centre) / half) ** 2)
        outer = x if x < panel_x0 else w - 1 - x          # 0 at the cartridge edge
        return slope + round(slope * (1 - outer / (wing - 1)) ** 2)

    img = Image.new('RGBA', (w, h), CLEAR)
    px = img.load()

    # Silhouette with 1px rounded bottom corners.
    for x in range(w):
        for y in range(top_edge(x), h):
            if x in (0, w - 1) and y == h - 1:
                continue
            px[x, y] = BODY

    # Outline, highlight on the top edge.
    for x in range(w):
        px[x, top_edge(x)] = OUTLINE if x in (0, w - 1) else HIGHLIGHT
    for y in range(h):
        for x in range(w):
            if px[x, y][3] == 0:
                continue
            edge = any(
                not (0 <= x + dx < w and 0 <= y + dy < h) or px[x + dx, y + dy][3] == 0
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))
            )
            if edge and px[x, y] != HIGHLIGHT:
                px[x, y] = OUTLINE

    # Seams between wings and panel.
    for sx in (panel_x0 - 1, panel_x1 + 1):
        for y in range(top_edge(sx), h - 1):
            px[sx, y] = SEAM

    # L-shaped grip grooves near the bottom of each wing.
    for x in range(groove_w):
        px[x, groove_y] = SEAM
        px[w - 1 - x, groove_y] = SEAM
    for y in range(groove_y, h - 1):
        px[groove_w - 1, y] = SEAM
        px[w - groove_w, y] = SEAM

    # Label window: transparent rounded rectangle inside a 1px dark rim.
    for y in range(label_y, label_y + label_h):
        for x in range(label_x, label_x + label_w):
            inside = in_rounded_rect(x, y, label_x + 1, label_y + 1, label_w - 2, label_h - 2, radius - 1)
            px[x, y] = CLEAR if inside else RECESS

    img.save(out_path)
    print(f'{out_path}: {w}x{h}, label window at ({label_x},{label_y}) {label_w}x{label_h}')


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else 'assets/images'
    build(1.0, os.path.join(out_dir, 'cartridge.png'))
    build(1.3, os.path.join(out_dir, 'cartridge_large.png'))


if __name__ == '__main__':
    main()
