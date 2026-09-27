#!/usr/bin/env python3
"""Generate the pixel-art N64 cartridges drawn over every Library label.

Writes three sprites (RGBA, transparent background) into assets/images/:
  cartridge.png                unselected tiles
  cartridge_large.png          the selected tile: scaled to fill its 146px tile, with a
                               70x82 label window so the full-resolution 74x86 label
                               fits behind it (2px hidden on each side) without scaling
  cartridge_large_outline.png  white selection outline around the large cartridge's
                               silhouette, OUTLINE_OFFSET px bigger on every side
  cartridge_large_outline_favorite.png  the same outline in gold, for favorites

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
WHITE = (0xFF, 0xFF, 0xFF, 0xFF)
GOLD = (0xF2, 0xC2, 0x30, 0xFF)       # outline of a favorite
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

LARGE_WIDTH = 146       # the selected tile's width (CAROUSEL_SELECTED_TILE_SIZE)
LABEL_SOURCE_W, LABEL_SOURCE_H = 74, 86   # Analogue 3D label resolution
LABEL_BLEED = 2         # label pixels hidden under the cartridge on each side (CARTRIDGE_LABEL_BLEED)

OUTLINE_GAP = 0         # transparent pixels between the cartridge and its selection outline
OUTLINE_WIDTH = 4       # outline thickness
OUTLINE_OFFSET = OUTLINE_GAP + OUTLINE_WIDTH


def in_rounded_rect(x, y, rx, ry, rw, rh, radius):
    """True if pixel (x, y) is inside the rounded rectangle, testing the pixel centre."""
    cx = min(max(x + 0.5, rx + radius), rx + rw - radius)
    cy = min(max(y + 0.5, ry + radius), ry + rh - radius)
    return (x + 0.5 - cx) ** 2 + (y + 0.5 - cy) ** 2 <= radius ** 2


def build_outline(mask, w, h, out_path, color):
    """White ring OUTLINE_GAP..OUTLINE_OFFSET px outside the silhouette in `mask`."""
    ow, oh = w + 2 * OUTLINE_OFFSET, h + 2 * OUTLINE_OFFSET
    img = Image.new('RGBA', (ow, oh), CLEAR)
    px = img.load()
    solid = [(x, y) for y in range(h) for x in range(w) if mask[y][x]]
    for y in range(oh):
        for x in range(ow):
            sx, sy = x - OUTLINE_OFFSET, y - OUTLINE_OFFSET
            if 0 <= sx < w and 0 <= sy < h and mask[sy][sx]:
                continue
            d = min((sx - mx) ** 2 + (sy - my) ** 2 for mx, my in solid) ** 0.5
            if OUTLINE_GAP < d <= OUTLINE_OFFSET + 0.25:
                px[x, y] = color
    img.save(out_path)
    print(f'{out_path}: {ow}x{oh} (outline offset {OUTLINE_OFFSET})')


def build(scale, out_path, outline_path=None, label_size=None):
    s = lambda v: round(v * scale)
    w, h, wing = s(BASE_W), s(BASE_H), s(BASE_WING)
    panel_x0, panel_x1 = wing + 1, w - wing - 2          # centre panel columns (inclusive)
    label_w, label_h = label_size if label_size else (s(BASE_LABEL_W), s(BASE_LABEL_H))
    label_y = s(BASE_LABEL_Y)
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

    # Filled silhouette for the selection outline, taken before the label window is cut out.
    mask = [[px[x, y][3] != 0 for x in range(w)] for y in range(h)]

    # Label window: transparent rounded rectangle inside a 1px dark rim.
    for y in range(label_y, label_y + label_h):
        for x in range(label_x, label_x + label_w):
            inside = in_rounded_rect(x, y, label_x + 1, label_y + 1, label_w - 2, label_h - 2, radius - 1)
            px[x, y] = CLEAR if inside else RECESS

    img.save(out_path)
    print(f'{out_path}: {w}x{h}, label window at ({label_x},{label_y}) {label_w}x{label_h}')

    if outline_path:
        build_outline(mask, w, h, outline_path, WHITE)
        build_outline(mask, w, h, outline_path.replace('.png', '_favorite.png'), GOLD)


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else 'assets/images'
    build(1.0, os.path.join(out_dir, 'cartridge.png'))
    build(
        LARGE_WIDTH / BASE_W,
        os.path.join(out_dir, 'cartridge_large.png'),
        os.path.join(out_dir, 'cartridge_large_outline.png'),
        label_size=(LABEL_SOURCE_W - 2 * LABEL_BLEED, LABEL_SOURCE_H - 2 * LABEL_BLEED),
    )


if __name__ == '__main__':
    main()
