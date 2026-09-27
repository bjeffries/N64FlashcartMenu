#!/usr/bin/env python3
"""Draw the masks the game-loading eclipse is built from at run time.

Writes white images whose alpha is the shape's coverage (8x8 supersampled), which the menu tints
and alpha-blends, so their soft edges suit any background colour (the pre-rendered boot frames
are smoothed against black):

  assets/images/eclipse_disc.png   the sun and the moon: radius 28, centred on pixel (29, 29)
  assets/images/eclipse_ring.png   the corona: radii 29 to 32, centred on pixel (33, 33)

The sizes match boot_animation/make_eclipse_intro.py (SUN_RADIUS, CORONA_*_RADIUS).

Usage: scripts/make_eclipse_masks.py [out_dir=assets/images]
"""

import os
import sys

from PIL import Image

DISC_RADIUS = 28
RING_INNER, RING_OUTER = 29, 32
SUPERSAMPLE = 8


def mask(size, centre, inside):
    img = Image.new('LA', (size, size), (255, 0))
    px = img.load()
    for y in range(size):
        for x in range(size):
            hits = 0
            for j in range(SUPERSAMPLE):
                for i in range(SUPERSAMPLE):
                    dx = x + (i + 0.5) / SUPERSAMPLE - centre
                    dy = y + (j + 0.5) / SUPERSAMPLE - centre
                    hits += inside(dx * dx + dy * dy)
            px[x, y] = (255, round(255 * hits / (SUPERSAMPLE * SUPERSAMPLE)))
    return img


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else 'assets/images'
    disc_size = 2 * (DISC_RADIUS + 1)
    ring_size = 2 * (RING_OUTER + 1)
    images = {
        'eclipse_disc': mask(disc_size, disc_size / 2, lambda d2: d2 <= DISC_RADIUS ** 2),
        'eclipse_ring': mask(ring_size, ring_size / 2, lambda d2: RING_INNER ** 2 <= d2 <= RING_OUTER ** 2),
    }
    for name, img in images.items():
        path = os.path.join(out_dir, name + '.png')
        img.save(path)
        print(f'{path}: {img.width}x{img.height}')


if __name__ == '__main__':
    main()
