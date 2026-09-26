/**
 * @file carousel.c
 * @brief Library carousel: a row of cartridge tiles with the selected game's title
 * @ingroup ui_components
 */

#include <ctype.h>
#include <string.h>

#include "../labels.h"
#include "../ui_components.h"
#include "../fonts.h"
#include "constants.h"

#define LABEL_CACHE_SIZE    (16)

typedef struct {
    int32_t position;       // position in the sorted browser list, -1 when the slot is free
    surface_t *label;       // NULL when the entry has no label
} label_slot_t;

static label_slot_t label_cache[LABEL_CACHE_SIZE];
static bool label_cache_ready = false;
static sprite_t *cartridge = NULL;


static void label_cache_reset (void) {
    for (int i = 0; i < LABEL_CACHE_SIZE; i++) {
        if (label_cache_ready) {
            labels_free(label_cache[i].label);
        }
        label_cache[i].position = -1;
        label_cache[i].label = NULL;
    }
    label_cache_ready = true;
}

static surface_t *label_get (path_t *directory, entry_t *entry, int32_t position, int32_t selected) {
    int free_slot = -1;
    int farthest_slot = 0;
    int32_t farthest_distance = -1;

    for (int i = 0; i < LABEL_CACHE_SIZE; i++) {
        if (label_cache[i].position == position) {
            return label_cache[i].label;
        }
        if (label_cache[i].position < 0) {
            free_slot = i;
        } else {
            int32_t distance = abs(label_cache[i].position - selected);
            if (distance > farthest_distance) {
                farthest_distance = distance;
                farthest_slot = i;
            }
        }
    }

    int slot = (free_slot >= 0) ? free_slot : farthest_slot;
    labels_free(label_cache[slot].label);
    label_cache[slot].position = position;
    label_cache[slot].label = NULL;

    if (entry->type == ENTRY_TYPE_ROM) {
        path_t *path = path_clone_push(directory, entry->name);
        uint32_t id;
        if (labels_rom_id(path_get(path), &id)) {
            label_cache[slot].label = labels_load(
                id,
                CARTRIDGE_LABEL_WIDTH + (CARTRIDGE_LABEL_BLEED * 2),
                CARTRIDGE_LABEL_HEIGHT + (CARTRIDGE_LABEL_BLEED * 2)
            );
        }
        path_free(path);
    }

    return label_cache[slot].label;
}

/**
 * @brief Turn a file name like "Legend of Zelda, The - Majora's Mask (USA).z64"
 *        into a display title like "The Legend of Zelda - Majora's Mask".
 */
void ui_components_carousel_title (const char *name, bool directory, char *out, size_t out_size) {
    snprintf(out, out_size, "%s", name);

    if (!directory) {
        char *ext = strrchr(out, '.');
        if (ext && ext != out) {
            *ext = '\0';
        }
        char *tags = strstr(out, " (");
        char *brackets = strstr(out, " [");
        if (brackets && (!tags || brackets < tags)) {
            tags = brackets;
        }
        if (tags) {
            *tags = '\0';
        }
    }

    // No-Intro style "Name, The - Subtitle" -> "The Name - Subtitle"
    char *article = strstr(out, ", The");
    if (article && (article[5] == '\0' || article[5] == ' ')) {
        char rest[128];
        snprintf(rest, sizeof(rest), "%s", article + 5);
        *article = '\0';
        char base[128];
        snprintf(base, sizeof(base), "%s", out);
        snprintf(out, out_size, "The %s%s", base, rest);
    }
}

static void draw_tile_background (int x, int y, color_t color) {
    // Filled square with 2px rounded corners.
    ui_components_box_draw(x + 2, y, x + CAROUSEL_TILE_SIZE - 2, y + CAROUSEL_TILE_SIZE, color);
    ui_components_box_draw(x, y + 2, x + CAROUSEL_TILE_SIZE, y + CAROUSEL_TILE_SIZE - 2, color);
    ui_components_box_draw(x + 1, y + 1, x + CAROUSEL_TILE_SIZE - 1, y + CAROUSEL_TILE_SIZE - 1, color);
}

static void draw_folder_icon (int x, int y) {
    int w = 64, h = 46;
    int fx = x + (CAROUSEL_TILE_SIZE - w) / 2;
    int fy = y + (CAROUSEL_TILE_SIZE - h) / 2 + 4;
    ui_components_box_draw(fx, fy - 8, fx + 26, fy, CAROUSEL_FOLDER_COLOR);
    ui_components_box_draw(fx, fy, fx + w, fy + h, CAROUSEL_FOLDER_COLOR);
}

static void draw_tile (path_t *directory, entry_t *entry, int32_t position, int32_t selected, int x, int y) {
    draw_tile_background(x, y, (position == selected) ? CAROUSEL_TILE_SELECTED_COLOR : CAROUSEL_TILE_COLOR);

    if (entry->type == ENTRY_TYPE_DIR) {
        draw_folder_icon(x, y);
        return;
    }

    int cx = x + (CAROUSEL_TILE_SIZE - CARTRIDGE_WIDTH) / 2;
    int cy = y + (CAROUSEL_TILE_SIZE - CARTRIDGE_HEIGHT) / 2;
    int lx = cx + CARTRIDGE_LABEL_X;
    int ly = cy + CARTRIDGE_LABEL_Y;
    surface_t *label = label_get(directory, entry, position, selected);

    // The label goes underneath; the cartridge is an overlay with a rounded window cut out for it.
    if (label) {
        rdpq_mode_push();
            rdpq_set_mode_copy(false);
            rdpq_tex_blit(label, lx - CARTRIDGE_LABEL_BLEED, ly - CARTRIDGE_LABEL_BLEED, NULL);
        rdpq_mode_pop();
    } else {
        ui_components_box_draw(lx, ly, lx + CARTRIDGE_LABEL_WIDTH, ly + CARTRIDGE_LABEL_HEIGHT, CAROUSEL_PLACEHOLDER_COLOR);
    }

    if (cartridge) {
        rdpq_mode_push();
            rdpq_set_mode_copy(true);
            rdpq_sprite_blit(cartridge, cx, cy, NULL);
        rdpq_mode_pop();
    }
}

static void draw_tile_caption (entry_t *entry, int x) {
    char title[128];
    ui_components_carousel_title(entry->name, entry->type == ENTRY_TYPE_DIR, title, sizeof(title));
    for (char *c = title; *c; c++) {
        *c = toupper((unsigned char) (*c));
    }

    rdpq_text_printn(
        &(rdpq_textparms_t) {
            .style_id = STL_GRAY,
            .width = CAROUSEL_TILE_SIZE,
            .wrap = WRAP_ELLIPSES,
        },
        FNT_DEFAULT,
        x,
        CAROUSEL_CAPTION_Y,
        title,
        strlen(title)
    );
}

/**
 * @brief Forget cached labels (call when the directory listing changes).
 */
void ui_components_carousel_invalidate (void) {
    label_cache_reset();
}

/**
 * @brief Draw the carousel row and the selected entry's title.
 */
void ui_components_carousel_draw (path_t *directory, entry_t *list, int32_t entries, int32_t selected) {
    if (!label_cache_ready) {
        label_cache_reset();
    }
    if (!cartridge) {
        cartridge = sprite_load("rom:/cartridge.sprite");
    }

    if (entries == 0) {
        ui_components_main_text_draw(STL_GRAY, ALIGN_CENTER, VALIGN_TOP, "\n\n\n\n\nThis folder has no games");
        return;
    }

    for (int32_t i = 0; i < entries; i++) {
        int x = CAROUSEL_SELECTED_X + ((i - selected) * CAROUSEL_TILE_PITCH);
        if (i < selected) {
            x -= CAROUSEL_SELECTED_GAP;
        } else if (i > selected) {
            x += CAROUSEL_SELECTED_GAP;
        }
        if (x + CAROUSEL_TILE_SIZE <= 0 || x >= DISPLAY_WIDTH) {
            continue;
        }
        draw_tile(directory, &list[i], i, selected, x, CAROUSEL_TILE_Y);
        if (i != selected) {
            draw_tile_caption(&list[i], x);
        }
    }

    char title[128];
    entry_t *entry = &list[selected];
    ui_components_carousel_title(entry->name, entry->type == ENTRY_TYPE_DIR, title, sizeof(title));

    rdpq_text_printn(
        &(rdpq_textparms_t) {
            .style_id = STL_DEFAULT,
            .width = VISIBLE_AREA_X1 - CAROUSEL_SELECTED_X,
            .wrap = WRAP_ELLIPSES,
        },
        FNT_TITLE,
        CAROUSEL_SELECTED_X,
        CAROUSEL_TITLE_Y,
        title,
        strlen(title)
    );
}
