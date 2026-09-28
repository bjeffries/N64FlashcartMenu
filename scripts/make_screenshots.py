#!/usr/bin/env python3
"""Put game screenshots on the SD card, sized for the Library's info panel.

Drop images into a folder, named after the game with a number: "Banjo-Kazooie_1.png",
"Banjo-Kazooie_2.jpg", ... (a name without a number counts as _1). Any size or format Pillow
reads. For every ROM in ROMS_DIR (searched recursively) whose name matches, the screenshots are
centre-cropped to 4:3, resized to 224x168 and written as

    SD_ROOT/menu/metadata/<A>/<B>/<C>/<D>/screenshot_1.png, screenshot_2.png, ...

where ABCD is the ROM's game code (next to its metadata.ini). Each region's ROM of a game has
its own code and gets its own copy. Screenshots already there for that game are replaced.

Names are compared like make_metadata.py does, ignoring case, punctuation and tags such as
"(USA)", with "Legend of Zelda, The - X" read as "The Legend of Zelda X"; a leading "The" is
optional. The match must be exact, so "Mario Party" screenshots don't also go to Mario Party 2.
Screenshots that match no ROM are listed.

Usage: scripts/make_screenshots.py <screenshots_dir> <roms_dir> <sd_root>
"""

import glob
import os
import re
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_metadata import ROM_EXTENSIONS, game_code, normalise   # noqa: E402

WIDTH, HEIGHT = 224, 168                # GAME_INFO_SCREENSHOT_WIDTH / HEIGHT in constants.h
IMAGE_EXTENSIONS = ('.png', '.jpg', '.jpeg', '.bmp', '.gif', '.webp')


def keys (name):
    """Comparable forms of a title: normalised, with and without a leading "the"."""
    key = normalise(name)
    return {key, key[4:] if key.startswith('the ') else 'the ' + key}


def parse (file_name):
    """(game name, number) from "Game Name_2.png"; no number counts as 1."""
    stem = os.path.splitext(file_name)[0]
    match = re.fullmatch(r'(.*?)[ _-]+(\d+)', stem)
    return (match.group(1), int(match.group(2))) if match else (stem, 1)


def fit (path):
    """Centre-crop to 4:3 and resize to WIDTH x HEIGHT."""
    image = Image.open(path).convert('RGB')
    w, h = image.size
    if w * 3 > h * 4:           # too wide: crop the sides
        crop = (h * 4) // 3
        image = image.crop(((w - crop) // 2, 0, (w - crop) // 2 + crop, h))
    elif w * 3 < h * 4:         # too tall: crop top and bottom
        crop = (w * 3) // 4
        image = image.crop((0, (h - crop) // 2, w, (h - crop) // 2 + crop))
    return image.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)


def main ():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    shots_dir, roms_dir, sd_root = sys.argv[1:]

    # Screenshots by game: {game name: [(number, path), ...]}
    games = {}
    for file_name in sorted(os.listdir(shots_dir)):
        if file_name.lower().endswith(IMAGE_EXTENSIONS) and not file_name.startswith('.'):
            name, number = parse(file_name)
            games.setdefault(name, []).append((number, os.path.join(shots_dir, file_name)))

    used, written = set(), []
    for folder, _, files in os.walk(roms_dir):
        for file_name in sorted(files):
            if not file_name.lower().endswith(ROM_EXTENSIONS):
                continue
            code = game_code(os.path.join(folder, file_name))
            rom_keys = keys(os.path.splitext(file_name)[0])
            name = next((g for g in games if keys(g) & rom_keys), None)
            if not code or not name:
                continue
            used.add(name)

            out_dir = os.path.join(sd_root, 'menu', 'metadata', *code)
            os.makedirs(out_dir, exist_ok=True)
            for old in glob.glob(os.path.join(out_dir, 'screenshot_*.png')):
                os.remove(old)
            shots = sorted(games[name])
            for index, (_, path) in enumerate(shots, start=1):
                fit(path).save(os.path.join(out_dir, f'screenshot_{index}.png'), optimize=True)
            written.append(f'{code}  {file_name}  <-  {len(shots)} screenshot(s)')

    for line in written:
        print(f'  {line}')
    unused = sorted(set(games) - used)
    if unused:
        print(f'\nNo ROM matches {len(unused)} screenshot name(s):')
        for name in unused:
            print(f'  {name}  ({", ".join(os.path.basename(p) for _, p in sorted(games[name]))})')
    print(f'\nWrote screenshots for {len(written)} ROM(s) to {os.path.join(sd_root, "menu", "metadata")}')


if __name__ == '__main__':
    main()
