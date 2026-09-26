#!/usr/bin/env python3
"""Generate the pixel-art N64 cartridge drawn behind every Library label.

Writes assets/images/cartridge.png (RGBA, transparent background). The sprite is
an overlay: the label is drawn first, then the cartridge on top, and the label
shows through a transparent window at LABEL_X/LABEL_Y (LABEL_W x LABEL_H) with
rounded corners and a dark rim. Keep these in sync with CARTRIDGE_LABEL_* in
src/menu/ui_components/constants.h.

Usage: scripts/make_cartridge.py [out=assets/images/cartridge.png]
"""

import sys
from PIL import Image

W, H = 88, 62
WING = 15                       # width of each side wing, seam excluded
PANEL_X0, PANEL_X1 = WING + 1, W - WING - 2   # centre panel columns (inclusive)
LABEL_X, LABEL_Y, LABEL_W, LABEL_H = 22, 6, 44, 51
WINDOW_RADIUS = 4

BODY = (0xC4, 0xC4, 0xC4, 0xFF)
HIGHLIGHT = (0xD8, 0xD8, 0xD8, 0xFF)
SEAM = (0x8A, 0x8A, 0x8A, 0xFF)
OUTLINE = (0x9A, 0x9A, 0x9A, 0xFF)
RECESS = (0x6A, 0x6A, 0x6A, 0xFF)
CLEAR = (0, 0, 0, 0)


def top_edge(x):
    """First opaque row of column x: an arch over the panel, lower sloping wings."""
    if PANEL_X0 - 1 <= x <= PANEL_X1 + 1:
        centre = (PANEL_X0 + PANEL_X1) / 2
        half = (PANEL_X1 - PANEL_X0) / 2
        return round(2.4 * ((x - centre) / half) ** 2)
    outer = x if x < PANEL_X0 else W - 1 - x      # 0 at the cartridge edge
    return 4 + round(4 * (1 - outer / (WING - 1)) ** 2)


def in_rounded_rect(x, y, rx, ry, rw, rh, radius):
    """True if pixel (x, y) is inside the rounded rectangle, testing the pixel centre."""
    cx = min(max(x + 0.5, rx + radius), rx + rw - radius)
    cy = min(max(y + 0.5, ry + radius), ry + rh - radius)
    return (x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 <= radius ** 2


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else 'assets/images/cartridge.png'
    img = Image.new('RGBA', (W, H), CLEAR)
    px = img.load()

    # Silhouette with 1px rounded bottom corners.
    for x in range(W):
        for y in range(top_edge(x), H):
            if (x in (0, W - 1)) and y == H - 1:
                continue
            px[x, y] = BODY

    # Outline, highlight on the top edge.
    for x in range(W):
        top = top_edge(x)
        px[x, top] = OUTLINE if x in (0, W - 1) else HIGHLIGHT
    for y in range(H):
        for x in range(W):
            if px[x, y][3] == 0:
                continue
            edge = any(
                not (0 <= x + dx < W and 0 <= y + dy < H) or px[x + dx, y + dy][3] == 0
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))
            )
            if edge and px[x, y] != HIGHLIGHT:
                px[x, y] = OUTLINE

    # Seams between wings and panel.
    for sx in (PANEL_X0 - 1, PANEL_X1 + 1):
        for y in range(top_edge(sx), H - 1):
            px[sx, y] = SEAM

    # L-shaped grip grooves near the bottom of each wing.
    groove_y = H - 15
    for x in range(0, 5):
        px[x, groove_y] = SEAM
        px[W - 1 - x, groove_y] = SEAM
    for y in range(groove_y, H - 1):
        px[4, y] = SEAM
        px[W - 5, y] = SEAM

    # Label window: transparent rounded rectangle inside a 1px dark rim.
    for y in range(LABEL_Y, LABEL_Y + LABEL_H):
        for x in range(LABEL_X, LABEL_X + LABEL_W):
            inside = in_rounded_rect(x, y, LABEL_X + 1, LABEL_Y + 1, LABEL_W - 2, LABEL_H - 2, WINDOW_RADIUS - 1)
            px[x, y] = CLEAR if inside else RECESS

    img.save(out)
    print(f'{out}: {W}x{H}, label at ({LABEL_X},{LABEL_Y}) {LABEL_W}x{LABEL_H}')


if __name__ == '__main__':
    main()
