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

/** @brief Everything that differs between the selected (large) tile and the others. */
typedef struct {
    int tile_size;
    int cartridge_width;
    int cartridge_height;
    int label_x;
    int label_y;
    int label_width;
    int label_height;
    sprite_t *cartridge;
} tile_style_t;

static tile_style_t small_style = {
    CAROUSEL_TILE_SIZE, CARTRIDGE_WIDTH, CARTRIDGE_HEIGHT,
    CARTRIDGE_LABEL_X, CARTRIDGE_LABEL_Y, CARTRIDGE_LABEL_WIDTH, CARTRIDGE_LABEL_HEIGHT,
    NULL
};

static tile_style_t large_style = {
    CAROUSEL_SELECTED_TILE_SIZE, CARTRIDGE_LARGE_WIDTH, CARTRIDGE_LARGE_HEIGHT,
    CARTRIDGE_LARGE_LABEL_X, CARTRIDGE_LARGE_LABEL_Y, CARTRIDGE_LARGE_LABEL_WIDTH, CARTRIDGE_LARGE_LABEL_HEIGHT,
    NULL
};

typedef struct {
    int32_t position;       // position in the sorted browser list, -1 when the slot is free
    bool has_id;            // false when the entry isn't a ROM or couldn't be read
    uint32_t id;            // label ID
    bool large_loaded;      // the large label is only loaded once the entry has been selected
    surface_t *small;       // NULL when there is no label
    surface_t *large;
} label_slot_t;

static label_slot_t label_cache[LABEL_CACHE_SIZE];
static bool label_cache_ready = false;


static void label_cache_reset (void) {
    for (int i = 0; i < LABEL_CACHE_SIZE; i++) {
        if (label_cache_ready) {
            labels_free(label_cache[i].small);
            labels_free(label_cache[i].large);
        }
        label_cache[i] = (label_slot_t) { .position = -1 };
    }
    label_cache_ready = true;
}

static surface_t *load_label (uint32_t id, tile_style_t *style) {
    // Loaded slightly larger than the window so the label's printed edge hides under the cartridge.
    return labels_load(
        id,
        style->label_width + (CARTRIDGE_LABEL_BLEED * 2),
        style->label_height + (CARTRIDGE_LABEL_BLEED * 2)
    );
}

static surface_t *label_get (path_t *directory, entry_t *entry, int32_t position, int32_t selected, bool large) {
    label_slot_t *slot = NULL;
    label_slot_t *free_slot = NULL;
    label_slot_t *farthest_slot = &label_cache[0];
    int32_t farthest_distance = -1;

    for (int i = 0; i < LABEL_CACHE_SIZE && !slot; i++) {
        if (label_cache[i].position == position) {
            slot = &label_cache[i];
        } else if (label_cache[i].position < 0) {
            free_slot = &label_cache[i];
        } else {
            int32_t distance = abs(label_cache[i].position - selected);
            if (distance > farthest_distance) {
                farthest_distance = distance;
                farthest_slot = &label_cache[i];
            }
        }
    }

    if (!slot) {
        slot = free_slot ? free_slot : farthest_slot;
        labels_free(slot->small);
        labels_free(slot->large);
        *slot = (label_slot_t) { .position = position };

        if (entry->type == ENTRY_TYPE_ROM) {
            path_t *path = path_clone_push(directory, entry->name);
            slot->has_id = labels_rom_id(path_get(path), &slot->id);
            path_free(path);
        }
        if (slot->has_id) {
            slot->small = load_label(slot->id, &small_style);
        }
    }

    if (large && !slot->large_loaded) {
        slot->large_loaded = true;
        if (slot->small) {
            slot->large = load_label(slot->id, &large_style);
        }
    }

    return large ? slot->large : slot->small;
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

static void draw_tile_background (int x, int y, int size, color_t color) {
    // Filled square with 2px rounded corners.
    ui_components_box_draw(x + 2, y, x + size - 2, y + size, color);
    ui_components_box_draw(x, y + 2, x + size, y + size - 2, color);
    ui_components_box_draw(x + 1, y + 1, x + size - 1, y + size - 1, color);
}

static void draw_folder_icon (int x, int y, int size) {
    int w = (size * 4) / 7, h = (size * 2) / 5;
    int fx = x + (size - w) / 2;
    int fy = y + (size - h) / 2 + 4;
    ui_components_box_draw(fx, fy - 8, fx + (w * 2) / 5, fy, CAROUSEL_FOLDER_COLOR);
    ui_components_box_draw(fx, fy, fx + w, fy + h, CAROUSEL_FOLDER_COLOR);
}

static void draw_tile (path_t *directory, entry_t *entry, int32_t position, int32_t selected, int x, int y) {
    bool is_selected = (position == selected);
    tile_style_t *style = is_selected ? &large_style : &small_style;

    draw_tile_background(x, y, style->tile_size, is_selected ? CAROUSEL_TILE_SELECTED_COLOR : CAROUSEL_TILE_COLOR);

    if (entry->type == ENTRY_TYPE_DIR) {
        draw_folder_icon(x, y, style->tile_size);
        return;
    }

    int cx = x + (style->tile_size - style->cartridge_width) / 2;
    int cy = y + (style->tile_size - style->cartridge_height) / 2;
    int lx = cx + style->label_x;
    int ly = cy + style->label_y;
    surface_t *label = label_get(directory, entry, position, selected, is_selected);

    // The label goes underneath; the cartridge is an overlay with a rounded window cut out for it.
    if (label) {
        rdpq_mode_push();
            rdpq_set_mode_copy(false);
            rdpq_tex_blit(label, lx - CARTRIDGE_LABEL_BLEED, ly - CARTRIDGE_LABEL_BLEED, NULL);
        rdpq_mode_pop();
    } else {
        ui_components_box_draw(lx, ly, lx + style->label_width, ly + style->label_height, CAROUSEL_PLACEHOLDER_COLOR);
    }

    if (style->cartridge) {
        rdpq_mode_push();
            rdpq_set_mode_copy(true);
            rdpq_sprite_blit(style->cartridge, cx, cy, NULL);
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
    if (!small_style.cartridge) {
        small_style.cartridge = sprite_load("rom:/cartridge.sprite");
        large_style.cartridge = sprite_load("rom:/cartridge_large.sprite");
    }

    if (entries == 0) {
        ui_components_main_text_draw(STL_GRAY, ALIGN_CENTER, VALIGN_TOP, "\n\n\n\n\nThis folder has no games");
        return;
    }

    for (int32_t i = 0; i < entries; i++) {
        int x = CAROUSEL_SELECTED_X;
        int y = CAROUSEL_SMALL_TILE_Y;
        int size = CAROUSEL_TILE_SIZE;
        if (i < selected) {
            x -= CAROUSEL_SELECTED_GAP + CAROUSEL_TILE_SIZE + ((selected - i - 1) * CAROUSEL_TILE_PITCH);
        } else if (i > selected) {
            x += CAROUSEL_SELECTED_TILE_SIZE + CAROUSEL_SELECTED_GAP + ((i - selected - 1) * CAROUSEL_TILE_PITCH);
        } else {
            y = CAROUSEL_TILE_Y;
            size = CAROUSEL_SELECTED_TILE_SIZE;
        }
        if (x + size <= 0 || x >= DISPLAY_WIDTH) {
            continue;
        }
        draw_tile(directory, &list[i], i, selected, x, y);
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
