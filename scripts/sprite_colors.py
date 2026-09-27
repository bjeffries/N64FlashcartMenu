"""The fixed sprite colours, read from src/menu/ui_components/sprite_colors.h.

make_icons.py and make_cartridge.py use this so the icons and cartridges they draw match the
colours the menu code uses next to them (tab bar, folders). SPRITE_COLORS maps names without the
SPRITE_ prefix to (r, g, b, a) tuples, e.g. SPRITE_COLORS['BUTTON_A'].
"""

import os
import re

SPRITE_COLORS_H = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'menu', 'ui_components', 'sprite_colors.h')

_COMPONENT = r'\s*(0x[0-9A-Fa-f]+|\d+)\s*'
_DEFINE = re.compile(r'^#define\s+SPRITE_(\w+)\s+RGBA32\(' + ','.join([_COMPONENT] * 4) + r'\)', re.MULTILINE)


def load(path=SPRITE_COLORS_H):
    with open(path) as f:
        text = f.read()
    return {name: tuple(int(value, 0) for value in values) for name, *values in _DEFINE.findall(text)}


SPRITE_COLORS = load()
