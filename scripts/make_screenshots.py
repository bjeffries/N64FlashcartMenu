#!/usr/bin/env python3
"""Put game screenshots on the SD card, sized for the Library's info panel.

Put each game's screenshots in a folder named after the game, e.g. "banjo kazooie/title.png",
"banjo kazooie/gameplay1.png", ... Within a folder they're shown in this order: "title" first,
then by the number in the name (gameplay1, gameplay2, ..., or name_1, name_2, ...), then by name.
Loose files named after the game with a number also work: "Banjo-Kazooie_1.png". Any size or
format Pillow reads.

For every ROM in ROMS_DIR (searched recursively) that matches a game, its screenshots are
centre-cropped to 4:3, resized to 224x168 and written as

    SD_ROOT/menu/metadata/<A>/<B>/<C>/<D>/screenshot_1.png, screenshot_2.png, ...

where ABCD is the ROM's game code (next to its metadata.ini). Each region's ROM of a game has
its own code and gets its own copy. Screenshots already there for that game are replaced.

Names are compared like make_metadata.py does, ignoring case, punctuation, spaces and tags such
as "(USA)", with "Legend of Zelda, The - X" read as "The Legend of Zelda X"; a leading "The" is
optional ("doom64" matches "Doom 64 (USA)"). A name that matches no ROM exactly may match the
start of a ROM name ("1080" -> "1080 Snowboarding"), but one that does match exactly doesn't
spread to longer titles ("Mario Party" stays off Mario Party 2). Names that match no ROM are listed.

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
    """Comparable forms of a title: normalised without spaces, with and without a leading "the"."""
    key = normalise(name)
    other = key[4:] if key.startswith('the ') else 'the ' + key
    return {key.replace(' ', ''), other.replace(' ', '')}


def parse (file_name):
    """(game name, number) from "Game Name_2.png"; no number counts as 1."""
    stem = os.path.splitext(file_name)[0]
    match = re.fullmatch(r'(.*?)[ _-]+(\d+)', stem)
    return (match.group(1), int(match.group(2))) if match else (stem, 1)


def order (file_name):
    """Sort key within a game's folder: "title" first, then by the number in the name."""
    stem = os.path.splitext(file_name)[0].lower()
    number = re.search(r'(\d+)$', stem)
    return (0 if stem == 'title' else 1, int(number.group(1)) if number else 0, stem)


def find_screenshots (shots_dir):
    """{game name: [paths in display order]} from game folders and loose "Name_N" files."""
    games = {}
    for entry in sorted(os.listdir(shots_dir)):
        path = os.path.join(shots_dir, entry)
        if entry.startswith('.'):
            continue
        if os.path.isdir(path):
            files = [f for f in os.listdir(path) if f.lower().endswith(IMAGE_EXTENSIONS) and not f.startswith('.')]
            if files:
                games.setdefault(entry, []).extend(os.path.join(path, f) for f in sorted(files, key=order))
        elif entry.lower().endswith(IMAGE_EXTENSIONS):
            name, number = parse(entry)
            games.setdefault(name, []).append((number, path))
    # Loose files were collected as (number, path): sort and drop the numbers.
    return {name: [p for _, p in sorted(v)] if v and isinstance(v[0], tuple) else v for name, v in games.items()}


def match (games, roms):
    """{rom file name: game name}: exact matches, then prefixes for names with no exact match."""
    rom_keys = {rom: keys(os.path.splitext(os.path.basename(rom))[0]) for rom in roms}
    matches, exact = {}, set()
    for game in games:
        for rom, rk in rom_keys.items():
            if keys(game) & rk:
                matches[rom] = game
                exact.add(game)
    for game in games:
        if game in exact:
            continue
        for rom, rk in rom_keys.items():
            if rom not in matches and any(r.startswith(g) for g in keys(game) for r in rk):
                matches[rom] = game
    return matches


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

    games = find_screenshots(shots_dir)
    roms = [os.path.join(folder, f) for folder, _, files in os.walk(roms_dir) for f in sorted(files)
            if f.lower().endswith(ROM_EXTENSIONS)]
    matches = match(games, roms)

    written = []
    for rom in sorted(matches):
        code = game_code(rom)
        if not code:
            continue
        out_dir = os.path.join(sd_root, 'menu', 'metadata', *code)
        os.makedirs(out_dir, exist_ok=True)
        for old in glob.glob(os.path.join(out_dir, 'screenshot_*.png')):
            os.remove(old)
        shots = games[matches[rom]]
        for index, path in enumerate(shots, start=1):
            fit(path).save(os.path.join(out_dir, f'screenshot_{index}.png'), optimize=True)
        written.append(f'{code}  {os.path.basename(rom)}  <-  {matches[rom]} ({len(shots)})')

    for line in written:
        print(f'  {line}')
    unused = sorted(set(games) - set(matches.values()))
    if unused:
        print(f'\nNo ROM matches {len(unused)} game name(s):')
        for name in unused:
            print(f'  {name}')
    print(f'\nWrote screenshots for {len(written)} ROM(s) to {os.path.join(sd_root, "menu", "metadata")}')


if __name__ == '__main__':
    main()
