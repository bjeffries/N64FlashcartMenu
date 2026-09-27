/**
 * @file labels.c
 * @brief Cartridge label images from an Analogue 3D labels.db file
 * @ingroup menu
 *
 * labels.db layout (little-endian):
 *   0x0000  header
 *   0x0100  4096 x uint32 label IDs, sorted ascending, unused slots are 0xFFFFFFFF
 *   0x4100  one 25600 byte record per ID, in table order: 74x86 BGRA32 pixels + padding
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <miniz.h>

#include "labels.h"
#include "utils/utils.h"

#define DB_TABLE_OFFSET     (0x100)
#define DB_TABLE_SLOTS      (4096)
#define DB_IMAGE_OFFSET     (DB_TABLE_OFFSET + (DB_TABLE_SLOTS * 4))
#define DB_IMAGE_STRIDE     (25600)
#define DB_UNUSED_ID        (0xFFFFFFFF)
#define LABEL_BYTES         (LABEL_WIDTH * LABEL_HEIGHT * 4)
#define ROM_ID_BYTES        (0x2000)

static char *db_path_copy = NULL;
static uint32_t *ids = NULL;
static int id_count = 0;


static uint32_t read_le32 (const uint8_t *p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t) (p[3]) << 24);
}

bool labels_init (const char *db_path) {
    labels_deinit();

    FILE *f = fopen(db_path, "rb");
    if (!f) {
        return false;
    }

    uint8_t *table = malloc(DB_TABLE_SLOTS * 4);
    bool ok = table
        && (fseek(f, DB_TABLE_OFFSET, SEEK_SET) == 0)
        && (fread(table, DB_TABLE_SLOTS * 4, 1, f) == 1);
    fclose(f);

    if (ok) {
        ids = malloc(DB_TABLE_SLOTS * sizeof(uint32_t));
        ok = ids != NULL;
    }
    if (ok) {
        while (id_count < DB_TABLE_SLOTS) {
            uint32_t id = read_le32(&table[id_count * 4]);
            if (id == DB_UNUSED_ID) {
                break;
            }
            ids[id_count++] = id;
        }
        db_path_copy = strdup(db_path);
        debugf("[LABELS] %d labels in %s\n", id_count, db_path);
    }

    free(table);
    if (!ok) {
        labels_deinit();
    }
    return ok;
}

void labels_deinit (void) {
    free(ids);
    ids = NULL;
    id_count = 0;
    free(db_path_copy);
    db_path_copy = NULL;
}

bool labels_rom_id (const char *rom_path, uint32_t *id, char game_code[4]) {
    FILE *f = fopen(rom_path, "rb");
    if (!f) {
        return false;
    }

    uint8_t *buffer = malloc(ROM_ID_BYTES);
    size_t length = buffer ? fread(buffer, 1, ROM_ID_BYTES, f) : 0;
    fclose(f);

    if (length < 4) {
        free(buffer);
        return false;
    }

    // Labels are keyed on big-endian (.z64) data, so undo .v64 / .n64 byte orders first.
    if (buffer[0] == 0x37 && buffer[1] == 0x80) {
        for (size_t i = 0; i + 1 < length; i += 2) {
            uint8_t tmp = buffer[i]; buffer[i] = buffer[i + 1]; buffer[i + 1] = tmp;
        }
    } else if (buffer[0] == 0x40 && buffer[1] == 0x12) {
        for (size_t i = 0; i + 3 < length; i += 4) {
            uint8_t tmp0 = buffer[i], tmp1 = buffer[i + 1];
            buffer[i] = buffer[i + 3]; buffer[i + 1] = buffer[i + 2];
            buffer[i + 2] = tmp1; buffer[i + 3] = tmp0;
        }
    }

    *id = (uint32_t) mz_crc32(MZ_CRC32_INIT, buffer, length);
    if (game_code) {
        memcpy(game_code, (length >= 0x3F) ? &buffer[0x3B] : (const uint8_t *) "    ", 4);
    }
    free(buffer);
    return true;
}

static int find_id (uint32_t id) {
    int lo = 0, hi = id_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (ids[mid] == id) {
            return mid;
        } else if (ids[mid] < id) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return -1;
}

surface_t *labels_load (uint32_t id, int width, int height) {
    int index = ids ? find_id(id) : -1;
    if (index < 0) {
        return NULL;
    }

    FILE *f = fopen(db_path_copy, "rb");
    if (!f) {
        return NULL;
    }

    uint8_t *bgra = malloc(LABEL_BYTES);
    bool ok = bgra
        && (fseek(f, DB_IMAGE_OFFSET + (index * DB_IMAGE_STRIDE), SEEK_SET) == 0)
        && (fread(bgra, LABEL_BYTES, 1, f) == 1);
    fclose(f);

    surface_t *label = NULL;
    if (ok && (label = malloc(sizeof(surface_t)))) {
        *label = surface_alloc(FMT_RGBA16, width, height);
        // Box filter: each output pixel averages the source pixels it covers.
        for (int y = 0; y < height; y++) {
            int sy0 = (y * LABEL_HEIGHT) / height;
            int sy1 = MAX(sy0 + 1, ((y + 1) * LABEL_HEIGHT) / height);
            uint16_t *row = (uint16_t *) (label->buffer + (y * label->stride));
            for (int x = 0; x < width; x++) {
                int sx0 = (x * LABEL_WIDTH) / width;
                int sx1 = MAX(sx0 + 1, ((x + 1) * LABEL_WIDTH) / width);
                uint32_t r = 0, g = 0, b = 0, n = 0;
                for (int sy = sy0; sy < sy1; sy++) {
                    const uint8_t *src = &bgra[((sy * LABEL_WIDTH) + sx0) * 4];
                    for (int sx = sx0; sx < sx1; sx++, src += 4, n++) {
                        b += src[0]; g += src[1]; r += src[2];
                    }
                }
                row[x] = color_to_packed16(RGBA32(r / n, g / n, b / n, 0xFF));
            }
        }
        data_cache_hit_writeback(label->buffer, label->stride * height);
    }

    free(bgra);
    return label;
}

static void free_surface (void *arg) {
    surface_t *label = arg;
    surface_free(label);
    free(label);
}

void labels_free (surface_t *label) {
    if (label) {
        rdpq_call_deferred(free_surface, label);
    }
}
