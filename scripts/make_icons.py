#!/usr/bin/env python3
"""Generate the Library's controller button icons as pixel art.

Writes icons with black glyphs (RGBA, transparent background) into assets/images/:
round A, B, Z and C-button icons, and the tab bar's L / R ends (a pill that curves down into
the 3px bar the menu draws between them). Each button type has a soft fill colour that keeps
good contrast with the black glyph.
Letters are hand-drawn 7x9 pixel glyphs with 2px strokes, in the style of the
Analogue OS font (the font itself doesn't render cleanly this small).

Usage: scripts/make_icons.py [out_dir=assets/images]
"""

import os
import sys
from PIL import Image, ImageDraw

from sprite_colors import SPRITE_COLORS    # src/menu/ui_components/sprite_colors.h

GREEN = SPRITE_COLORS['BUTTON_A']
RED = SPRITE_COLORS['BUTTON_B']
YELLOW = SPRITE_COLORS['BUTTON_C']
LIGHT_GREY = SPRITE_COLORS['BUTTON_GRAY']   # L, R, Z (and the tab bar: TAB_BAR_COLOR)
BLACK = SPRITE_COLORS['BUTTON_GLYPH']
CLEAR = (0, 0, 0, 0)

ROUND_SIZE = 18                 # diameter of A / B / C buttons
PILL_W, PILL_H = 20, 16         # L / R shoulder buttons
TAB_PILL_W = 26                 # tab bar ends: longer than a button, like a trigger
TAB_LETTER_INSET = TAB_PILL_W - PILL_W  # the letter sits centred in the pill's inner 20px
TAB_END_W = TAB_PILL_W + 8      # pill plus the curve down into the bar
TAB_BAR_H = 3                   # bar thickness; its bottom lines up with the pill's
PILL_RADIUS = 4
SUPERSAMPLE = 8

GLYPHS = {
    'A': ['..###..', '.##.##.', '##...##', '##...##', '#######', '##...##', '##...##', '##...##', '##...##'],
    'B': ['######.', '##...##', '##...##', '##...##', '######.', '##...##', '##...##', '##...##', '######.'],
    'L': ['##.....', '##.....', '##.....', '##.....', '##.....', '##.....', '##.....', '##.....', '######.'],
    'R': ['######.', '##...##', '##...##', '##...##', '######.', '##.##..', '##..##.', '##...##', '##...##'],
    'Z': ['#######', '.....##', '....##.', '...##..', '..##...', '.##....', '##.....', '##.....', '#######'],
}


def circle(size, color):
    img = Image.new('RGBA', (size, size), CLEAR)
    px = img.load()
    r = size / 2
    for y in range(size):
        for x in range(size):
            if (x + 0.5 - r) ** 2 + (y + 0.5 - r) ** 2 <= r * r:
                px[x, y] = color
    return img


def pill(w, h, color):
    img = Image.new('RGBA', (w, h), CLEAR)
    ImageDraw.Draw(img).rounded_rectangle((0, 0, w - 1, h - 1), radius=4, fill=color)
    return img


def letter(img, char, area_x=0, area_w=None):
    """Draw a glyph centred in the image (or in the columns area_x .. area_x + area_w)."""
    rows = GLYPHS[char]
    width = max(len(r.rstrip('.')) for r in rows)
    area_w = img.width if area_w is None else area_w
    x0 = area_x + (area_w - width + 1) // 2
    y0 = (img.height - len(rows) + 1) // 2
    px = img.load()
    for y, row in enumerate(rows):
        for x, c in enumerate(row):
            if c == '#':
                px[x0 + x, y0 + y] = BLACK
    return img


def tab_end_l():
    """Left end of the tab bar: an L pill whose right side curves down into the bar.

    Solid gray on a transparent background: RGBA16 sprites only have 1-bit alpha, so a pixel is
    gray if the shape covers at least half of it (darker edge pixels showed as a dark border on
    coloured palettes). The right end is this shape mirrored.
    """
    w, h, ss = TAB_END_W, PILL_H, SUPERSAMPLE
    bar_top = h - TAB_BAR_H
    curve_x0, curve_x1 = TAB_PILL_W - 3, TAB_END_W    # where the top edge leaves the pill / meets the bar

    def inside(x, y):
        if x < PILL_RADIUS:                         # rounded left corners
            dy = max(0, PILL_RADIUS - y, y - (h - PILL_RADIUS))
            return (x - PILL_RADIUS) ** 2 + dy ** 2 <= PILL_RADIUS ** 2 and 0 <= y < h
        if x < curve_x0:
            return 0 <= y < h
        t = min(1.0, (x - curve_x0) / (curve_x1 - curve_x0))
        top = bar_top * (t * t * (3 - 2 * t))       # smooth S from the pill's top to the bar's
        return top <= y < h

    img = Image.new('RGBA', (w, h), CLEAR)
    px = img.load()
    for y in range(h):
        for x in range(w):
            hits = sum(inside(x + (i + 0.5) / ss, y + (j + 0.5) / ss) for i in range(ss) for j in range(ss))
            coverage = hits / (ss * ss)
            if coverage >= 0.5:
                px[x, y] = LIGHT_GREY
    return img


def triangle(img, direction):
    """Solid pixel triangle pointing right, left, up or down, centred in the icon."""
    px = img.load()
    cx, cy = img.width // 2, img.height // 2
    for i in range(5):              # 5 rows/columns from the base to the tip
        for j in range(-4 + i, 5 - i):
            if direction == 'right':
                px[cx - 2 + i, cy + j] = BLACK
            elif direction == 'left':
                px[cx + 1 - i, cy + j] = BLACK
            elif direction == 'up':
                px[cx + j, cy + 2 - i] = BLACK
            else:
                px[cx + j, cy - 2 + i] = BLACK
    return img


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else 'assets/images'

    icons = {
        'button_a': letter(circle(ROUND_SIZE, GREEN), 'A'),
        'button_b': letter(circle(ROUND_SIZE, RED), 'B'),
        'button_c_right': triangle(circle(ROUND_SIZE, YELLOW), 'right'),
        'button_c_up': triangle(circle(ROUND_SIZE, YELLOW), 'up'),
        'button_c_down': triangle(circle(ROUND_SIZE, YELLOW), 'down'),
        'button_c_left': triangle(circle(ROUND_SIZE, YELLOW), 'left'),
        'button_z': letter(circle(ROUND_SIZE, LIGHT_GREY), 'Z'),
        'tab_l': letter(tab_end_l(), 'L', TAB_LETTER_INSET, PILL_W),
        'tab_r': letter(tab_end_l().transpose(Image.FLIP_LEFT_RIGHT), 'R', TAB_END_W - TAB_PILL_W, PILL_W),
    }
    for name, img in icons.items():
        path = os.path.join(out_dir, name + '.png')
        img.save(path)
        print(f'{path}: {img.width}x{img.height}')


if __name__ == '__main__':
    main()
