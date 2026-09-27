#!/usr/bin/env python3
"""Write per-game metadata for the Library info panel from a JSON list of games.

For every ROM in ROMS_DIR (searched recursively) this reads the 4-character game code
from the ROM header, matches the file name to a game in the JSON, and writes

    SD_ROOT/menu/metadata/<A>/<B>/<C>/<D>/metadata.ini

which the menu reads for the title, developer, publisher, release year, player count
and description. Different regions of a game (e.g. USA and PAL) have different game
codes and all get the same data.

JSON: a list of objects with title, developer, publisher, release_year, player_count and
description. Characters the menu font doesn't have are replaced (é -> e, — -> -, curly
quotes -> straight) or dropped (®, ™, °).

Usage: scripts/make_metadata.py <games.json> <roms_dir> <sd_root>
"""

import json
import os
import re
import sys

ROM_EXTENSIONS = ('.z64', '.n64', '.v64')

REPLACEMENTS = {
    '\u00e9': 'e', '\u00c9': 'E', '\u00e8': 'e', '\u00e0': 'a', '\u00f6': 'o', '\u00fc': 'u',
    '\u2014': ' - ', '\u2013': '-',
    '\u2018': "'", '\u2019': "'", '\u201c': '"', '\u201d': '"',
    '\u2026': '...',
    '\u00ae': '', '\u2122': '', '\u00b0': '',
}


def to_font_text(text):
    """Replace characters the Analogue OS font doesn't have."""
    text = ''.join(REPLACEMENTS.get(c, c) for c in text)
    text = re.sub(r' {2,}', ' ', text)
    return text.encode('ascii', 'ignore').decode('ascii')


def normalise(title):
    """Comparable form of a title or file name: lower case, no tags, no punctuation."""
    title = to_font_text(title)
    title = re.sub(r'\s*[\(\[].*$', '', title)                      # "(USA) (Rev 1)", "[!]"
    title = re.sub(r'^(.*?), The\b(.*)$', r'The \1\2', title)       # "Legend of Zelda, The - X"
    title = title.lower().replace('&', ' and ')
    title = re.sub(r"[^a-z0-9]+", ' ', title.replace("'", ''))
    return ' '.join(title.split())


def game_code(rom_path):
    with open(rom_path, 'rb') as f:
        header = bytearray(f.read(0x40))
    if len(header) < 0x40:
        return None
    if header[:4] == b'\x37\x80\x40\x12':            # .v64 byte-swapped
        header = bytearray(b for i in range(0, 0x40, 2) for b in (header[i + 1], header[i]))
    elif header[:4] == b'\x40\x12\x37\x80':          # .n64 little-endian
        header = bytearray(b''.join(bytes(header[i:i + 4])[::-1] for i in range(0, 0x40, 4)))
    code = header[0x3B:0x3F].decode('ascii', 'replace')
    return code if re.fullmatch(r'[A-Za-z0-9]{4}', code) else None


def find_game(file_name, games):
    """The game whose title the file name starts with (longest match wins)."""
    name = normalise(os.path.splitext(file_name)[0])
    best = None
    for key, game in games.items():
        if name == key or name.startswith(key + ' '):
            if best is None or len(key) > len(best[0]):
                best = (key, game)
    return best[1] if best else None


def ini_quote(value):
    value = str(value).replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n')
    return f'"{value}"'


def write_metadata(path, game):
    lines = [
        '[meta]',
        f'name = {ini_quote(to_font_text(game["title"]))}',
        f'author = {ini_quote(to_font_text(game["developer"]))}',
        f'publisher = {ini_quote(to_font_text(game["publisher"]))}',
        f'release-date = {ini_quote(game["release_year"])}',
        f'num-players = {int(game["player_count"])}',
        f'short-desc = {ini_quote(to_font_text(game["description"]))}',
    ]
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    json_path, roms_dir, sd_root = sys.argv[1:]

    games = {normalise(g['title']): g for g in json.load(open(json_path, encoding='utf-8'))}

    matched, unmatched, no_code = [], [], []
    for folder, _, files in os.walk(roms_dir):
        for file_name in sorted(files):
            if not file_name.lower().endswith(ROM_EXTENSIONS):
                continue
            rom_path = os.path.join(folder, file_name)
            code = game_code(rom_path)
            if not code:
                no_code.append(file_name)
                continue
            game = find_game(file_name, games)
            if not game:
                unmatched.append(f'{code}  {file_name}')
                continue
            write_metadata(os.path.join(sd_root, 'menu', 'metadata', *code, 'metadata.ini'), game)
            matched.append(f'{code}  {file_name}  ->  {to_font_text(game["title"])}')

    for line in matched:
        print(f'  {line}')
    if unmatched:
        print(f'\nNo metadata for {len(unmatched)} ROM(s):')
        for line in unmatched:
            print(f'  {line}')
    if no_code:
        print(f'\nCouldn\'t read a game code from {len(no_code)} file(s):')
        for line in no_code:
            print(f'  {line}')
    print(f'\nWrote metadata for {len(matched)} ROM(s) to {os.path.join(sd_root, "menu", "metadata")}')


if __name__ == '__main__':
    main()
