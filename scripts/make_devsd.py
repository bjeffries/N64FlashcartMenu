#!/usr/bin/env python3
"""Build a small fake SD card (devsd/) for testing the menu in an emulator.

Emulators can't emulate the SC64 SD card, so a DEV_SD=1 build packs devsd/
into the menu's own ROM filesystem (rom:/) instead.

For every ROM in ROMS_DIR this writes an 8 KiB stub (the header plus enough
data to compute the Analogue 3D label ID) and a trimmed labels.db holding only
the labels for those ROMs.

Usage: scripts/make_devsd.py <roms_dir> <labels.db> [out_dir=devsd]
"""

import os
import shutil
import struct
import sys
import zlib

LABEL_ID_BYTES = 0x2000
DB_TABLE_OFFSET = 0x100
DB_TABLE_SLOTS = 4096
DB_IMAGE_OFFSET = DB_TABLE_OFFSET + DB_TABLE_SLOTS * 4
DB_IMAGE_STRIDE = 25600
ROM_EXTENSIONS = ('.z64', '.n64', '.v64')


def to_big_endian(data):
    magic = data[:4]
    if magic == b'\x37\x80\x40\x12':  # byte-swapped (.v64)
        return bytes(b for i in range(0, len(data), 2) for b in (data[i + 1], data[i]))
    if magic == b'\x40\x12\x37\x80':  # little-endian (.n64)
        return b''.join(data[i:i + 4][::-1] for i in range(0, len(data), 4))
    return data


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    roms_dir, db_path = sys.argv[1], sys.argv[2]
    out_dir = sys.argv[3] if len(sys.argv) > 3 else 'devsd'

    db = open(db_path, 'rb').read()
    ids = struct.unpack_from(f'<{DB_TABLE_SLOTS}I', db, DB_TABLE_OFFSET)
    index_of = {label_id: i for i, label_id in enumerate(ids) if label_id != 0xFFFFFFFF}

    shutil.rmtree(out_dir, ignore_errors=True)
    games_dir = os.path.join(out_dir, 'N64')
    os.makedirs(games_dir)
    os.makedirs(os.path.join(out_dir, 'menu'))

    found = {}
    for name in sorted(os.listdir(roms_dir)):
        if not name.lower().endswith(ROM_EXTENSIONS):
            continue
        head = open(os.path.join(roms_dir, name), 'rb').read(LABEL_ID_BYTES)
        open(os.path.join(games_dir, name), 'wb').write(head)
        label_id = zlib.crc32(to_big_endian(head))
        status = 'label' if label_id in index_of else 'NO LABEL'
        print(f'{status:8}  {label_id:08x}  {name}')
        if label_id in index_of:
            found[label_id] = index_of[label_id]

    table = sorted(found)
    out = bytearray(db[:DB_TABLE_OFFSET])
    out += struct.pack(f'<{len(table)}I', *table)
    out += b'\xff' * 4 * (DB_TABLE_SLOTS - len(table))
    for label_id in table:
        start = DB_IMAGE_OFFSET + found[label_id] * DB_IMAGE_STRIDE
        out += db[start:start + DB_IMAGE_STRIDE]
    open(os.path.join(out_dir, 'menu', 'labels.db'), 'wb').write(out)
    print(f'\n{len(table)} labels -> {out_dir}/menu/labels.db ({len(out)} bytes)')


if __name__ == '__main__':
    main()
