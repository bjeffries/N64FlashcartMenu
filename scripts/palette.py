"""The UI palette, read from src/menu/ui_components/palette.h.

The sprite scripts use this so the icons and cartridges they draw match the colours the menu
code uses. PALETTE maps names without the PALETTE_ prefix to (r, g, b, a) tuples, e.g.
PALETTE['GREEN'] or PALETTE['GRAY_12'].
"""

import os
import re

PALETTE_H = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'menu', 'ui_components', 'palette.h')

_COMPONENT = r'\s*(0x[0-9A-Fa-f]+|\d+)\s*'
_DEFINE = re.compile(r'^#define\s+PALETTE_(\w+)\s+RGBA32\(' + ','.join([_COMPONENT] * 4) + r'\)', re.MULTILINE)


def load(path=PALETTE_H):
    with open(path) as f:
        text = f.read()
    return {name: tuple(int(value, 0) for value in values) for name, *values in _DEFINE.findall(text)}


PALETTE = load()
