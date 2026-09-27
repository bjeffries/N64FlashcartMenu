#!/usr/bin/env python3
"""Generate the Library's controller button icons as pixel art.

Writes white icons with black glyphs (RGBA, transparent background) into
assets/images/: round A, B, Z and C-button icons, and L / R shoulder pills.
Letters are hand-drawn 7x9 pixel glyphs with 2px strokes, in the style of the
Analogue OS font (the font itself doesn't render cleanly this small).

Usage: scripts/make_icons.py [out_dir=assets/images]
"""

import os
import sys
from PIL import Image, ImageDraw

WHITE = (0xFF, 0xFF, 0xFF, 0xFF)
BLACK = (0x00, 0x00, 0x00, 0xFF)
CLEAR = (0, 0, 0, 0)

ROUND_SIZE = 18                 # diameter of A / B / C buttons
PILL_W, PILL_H = 20, 16         # L / R shoulder buttons

GLYPHS = {
    'A': ['..###..', '.##.##.', '##...##', '##...##', '#######', '##...##', '##...##', '##...##', '##...##'],
    'B': ['######.', '##...##', '##...##', '##...##', '######.', '##...##', '##...##', '##...##', '######.'],
    'L': ['##.....', '##.....', '##.....', '##.....', '##.....', '##.....', '##.....', '##.....', '######.'],
    'R': ['######.', '##...##', '##...##', '##...##', '######.', '##.##..', '##..##.', '##...##', '##...##'],
    'Z': ['#######', '.....##', '....##.', '...##..', '..##...', '.##....', '##.....', '##.....', '#######'],
}


def circle(size):
    img = Image.new('RGBA', (size, size), CLEAR)
    px = img.load()
    r = size / 2
    for y in range(size):
        for x in range(size):
            if (x + 0.5 - r) ** 2 + (y + 0.5 - r) ** 2 <= r * r:
                px[x, y] = WHITE
    return img


def pill(w, h):
    img = Image.new('RGBA', (w, h), CLEAR)
    ImageDraw.Draw(img).rounded_rectangle((0, 0, w - 1, h - 1), radius=4, fill=WHITE)
    return img


def letter(img, char):
    rows = GLYPHS[char]
    width = max(len(r.rstrip('.')) for r in rows)
    x0 = (img.width - width + 1) // 2
    y0 = (img.height - len(rows) + 1) // 2
    px = img.load()
    for y, row in enumerate(rows):
        for x, c in enumerate(row):
            if c == '#':
                px[x0 + x, y0 + y] = BLACK
    return img


def triangle(img, direction):
    """Solid pixel triangle pointing right, left or up, centred in the icon."""
    px = img.load()
    cx, cy = img.width // 2, img.height // 2
    for i in range(5):              # 5 rows/columns from the base to the tip
        for j in range(-4 + i, 5 - i):
            if direction == 'right':
                px[cx - 2 + i, cy + j] = BLACK
            elif direction == 'left':
                px[cx + 1 - i, cy + j] = BLACK
            else:
                px[cx + j, cy + 2 - i] = BLACK
    return img


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else 'assets/images'

    icons = {
        'button_a': letter(circle(ROUND_SIZE), 'A'),
        'button_b': letter(circle(ROUND_SIZE), 'B'),
        'button_c_right': triangle(circle(ROUND_SIZE), 'right'),
        'button_c_up': triangle(circle(ROUND_SIZE), 'up'),
        'button_c_left': triangle(circle(ROUND_SIZE), 'left'),
        'button_z': letter(circle(ROUND_SIZE), 'Z'),
        'button_l': letter(pill(PILL_W, PILL_H), 'L'),
        'button_r': letter(pill(PILL_W, PILL_H), 'R'),
    }
    for name, img in icons.items():
        path = os.path.join(out_dir, name + '.png')
        img.save(path)
        print(f'{path}: {img.width}x{img.height}')


if __name__ == '__main__':
    main()
