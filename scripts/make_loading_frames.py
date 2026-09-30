#!/usr/bin/env python3
"""Render the game-loading eclipse once per palette background.

The loading animation plays the boot animation's eclipse (frames 1-57, no title) over the
Library. Frames smoothed against black leave dark fringes on a coloured background, so this
renders the frames against each distinct palette background (from palette.c), with the same
geometry and timing as boot_animation/make_eclipse_intro.py:

  - the geometry is exactly the boot animation's (title_mask() places the sun, as it does for
    the boot frames) and the colours are the boot animation's in every set (the moon goes from
    grey to the Eclipse background, #231A2E); only the smoothing of the edges differs;
  - in the last 20% of loading the menu fades the screen to black (LOADING_FADE_START), so each
    of those frames is smoothed against the background it will be drawn on, and the corona
    frames (after loading) against black.

Only the frames the menu uses are written (it skips every other approach frame; see
loading_animation.c). Output, per set (named after the first palette with that background):

  assets/loading/<set>/NN.png          cropped, paletted, background colour transparent
  src/menu/loading_frames.h            the sun's centre in the frames, and per set: background
                                       colour, and per frame the image to draw and where

Usage: scripts/make_loading_frames.py
"""

import importlib.util
import os
import re
import shutil
import sys

from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
INTRO = os.path.join(ROOT, 'boot_animation', 'make_eclipse_intro.py')
PALETTE_C = os.path.join(ROOT, 'src', 'menu', 'ui_components', 'palette.c')
OUT_IMAGES = os.path.join(ROOT, 'assets', 'loading')
OUT_HEADER = os.path.join(ROOT, 'src', 'menu', 'loading_frames.h')

FRAMES = 57                 # 1-9 fade in, 10-54 approach, 55-57 corona
APPROACH_FIRST, APPROACH_LAST = 10, 54
LOADING_FADE_START = 0.8    # constants.h: from here the screen fades to black


def load_intro():
    spec = importlib.util.spec_from_file_location('intro', INTRO)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def palette_backgrounds():
    """(set name, background) for each distinct palette background, in palette.c order."""
    text = open(PALETTE_C).read()
    sets = []
    for key, rgb in re.findall(r'\.key = "(\w+)",\s*\.background = \{ (0x[0-9A-Fa-f]+, 0x[0-9A-Fa-f]+, 0x[0-9A-Fa-f]+)', text):
        colour = tuple(int(v, 16) for v in rgb.split(', '))
        if colour not in [c for _, c in sets]:
            sets.append((key, colour))
    return sets


def frame_used(frame):
    """Must match frame_used() in loading_animation.c."""
    if APPROACH_FIRST < frame < APPROACH_LAST:
        return (frame - APPROACH_FIRST) % 2 == 0
    return frame <= FRAMES


def mix(a, b, amount):
    return tuple(round(x + (y - x) * amount) for x, y in zip(a, b))


def frame_background(frame, background):
    """The background this frame is drawn on: faded towards black at the end of loading."""
    if frame > APPROACH_LAST:
        return (0, 0, 0)
    if frame < APPROACH_FIRST:
        return background
    progress = (frame - APPROACH_FIRST) / (APPROACH_LAST - APPROACH_FIRST)
    fade = min(1.0, max(0.0, (progress - LOADING_FADE_START) / (1.0 - LOADING_FADE_START)))
    return mix(background, (0, 0, 0), fade)


def render(E, frame, background, sun_mask, corona_mask):
    """Frame of the boot animation (no title) smoothed against background."""
    centre, p, fade, corona = E.state(frame)
    image = Image.new('RGB', sun_mask.size, background)
    if frame <= APPROACH_LAST:
        image.paste(mix(background, E.colour_lerp(E.SUN_START_COLOUR, E.SUN_END_COLOUR, p), fade), (0, 0), sun_mask)
    moon_mask = sun_mask if centre == E.SUN_CENTRE else E.disc_mask(centre, E.MOON_RADIUS)
    moon = E.colour_lerp(E.MOON_START_COLOUR, E.MOON_END_COLOUR, p)     # grey to #231A2E, as on the boot animation
    image.paste(mix(background, moon, fade), (0, 0), moon_mask)
    if corona > 0:
        image.paste(mix(background, E.CORONA_COLOUR, corona * fade), (0, 0), corona_mask)
    return image.resize((E.WIDTH, E.HEIGHT), Image.Resampling.BOX)


def paletted(image, background):
    """Exact palette, background first (index 0, transparent)."""
    colours = image.getcolors(256)
    if colours is None:
        sys.exit('A frame has more than 256 colours')
    palette = sorted((c for _, c in colours), key=lambda c: c != background)
    if palette[0] != background:
        palette.insert(0, background)
    index = {c: i for i, c in enumerate(palette)}
    rgb = image.tobytes()
    out = Image.frombytes('P', image.size, bytes(index[tuple(rgb[i:i + 3])] for i in range(0, len(rgb), 3)))
    out.putpalette([v for c in palette for v in c])
    return out


def main():
    E = load_intro()
    E.title_mask()      # places the sun exactly as on the boot animation (sets E.SUN_CENTRE)
    sun_mask = E.disc_mask(E.SUN_CENTRE, E.SUN_RADIUS)
    corona_mask = E.disc_mask(E.SUN_CENTRE, E.CORONA_OUTER_RADIUS)
    corona_mask.paste(0, (0, 0), E.disc_mask(E.SUN_CENTRE, E.CORONA_INNER_RADIUS))

    shutil.rmtree(OUT_IMAGES, ignore_errors=True)
    sets = []
    for name, background in palette_backgrounds():
        folder = os.path.join(OUT_IMAGES, name)
        os.makedirs(folder)
        images, table = {}, []
        for frame in range(1, FRAMES + 1):
            if not frame_used(frame):
                table.append((-1, 0, 0))
                continue
            bg = frame_background(frame, background)
            image = render(E, frame, bg, sun_mask, corona_mask)
            # Crop to what differs from the background (8-pixel multiple width, even x).
            bbox = Image.frombytes('L', image.size, bytes(
                0 if px == bg else 255 for px in image.getdata())).getbbox()
            if bbox is None:
                table.append((-1, 0, 0))
                continue
            x0, y0, x1, y1 = bbox
            x0 -= x0 % 2
            x1 = min(image.width, x0 + (x1 - x0 + 7) // 8 * 8)
            crop = image.crop((x0, y0, x1, y1))
            key = (x0, y0, crop.tobytes())
            if key not in images:
                images[key] = len(images)
                out = paletted(crop, bg)
                out.save(os.path.join(folder, f'{images[key]:02d}.png'), transparency=0)
            table.append((images[key], x0, y0))
        sets.append((name, background, len(images), table))
        print(f'{name} {background}: {len(images)} images')

    lines = [
        '// Generated by scripts/make_loading_frames.py - do not edit.',
        '',
        '#ifndef LOADING_FRAMES_H__',
        '#define LOADING_FRAMES_H__',
        '',
        '#include <stdint.h>',
        '',
        f'#define LOADING_FRAME_COUNT     ({FRAMES})',
        f'#define LOADING_FRAME_SETS      ({len(sets)})',
        f'#define LOADING_MAX_IMAGES      ({max(s[2] for s in sets)})',
        '',
        '/** @brief The sun\'s centre in the frames\' coordinates (the boot animation\'s, rounded). */',
        f'#define LOADING_SUN_X           ({round(E.SUN_CENTRE[0])})',
        f'#define LOADING_SUN_Y           ({round(E.SUN_CENTRE[1])})',
        '',
        '/** @brief One set of loading frames, smoothed against a palette background (rom:/loading/<dir>/NN.sprite). */',
        'typedef struct {',
        '    const char *dir;',
        '    uint8_t r, g, b;                        // the palette background it was rendered for',
        '    int images;',
        '    struct { int8_t image; int16_t x; int16_t y; } frames[LOADING_FRAME_COUNT];   // -1: nothing to draw',
        '} loading_frame_set_t;',
        '',
        'static const loading_frame_set_t loading_frame_sets[LOADING_FRAME_SETS] = {',
    ]
    for name, (r, g, b), count, table in sets:
        lines.append(f'    {{ "{name}", 0x{r:02X}, 0x{g:02X}, 0x{b:02X}, {count}, {{')
        lines += [f'        {{ {i}, {x}, {y} }},' for i, x, y in table]
        lines.append('    } },')
    lines += ['};', '', '#endif', '']
    with open(OUT_HEADER, 'w', newline='\n') as f:
        f.write('\n'.join(lines))


if __name__ == '__main__':
    main()
