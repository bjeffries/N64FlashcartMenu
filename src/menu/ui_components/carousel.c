/**
 * @file carousel.c
 * @brief Library carousel: a row of cartridge tiles with the selected game's title
 * @ingroup ui_components
 */

#include <ctype.h>
#include <math.h>
#include <string.h>

#include "../labels.h"
#include "../sound.h"
#include "../ui_components.h"
#include "../fonts.h"
#include "constants.h"
#include "utils/utils.h"

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
    bool has_title;         // title from the game's metadata.ini (menu/metadata/A/B/C/D)
    char title[64];
} label_slot_t;

static label_slot_t label_cache[LABEL_CACHE_SIZE];
static bool label_cache_ready = false;

static sprite_t *selection_outline = NULL;
static sprite_t *favorite_outline = NULL;
static sprite_t *hidden_outline = NULL;

static bool scroll_ready = false;       // false: snap to the selection on the next draw
static float scroll_position = 0;       // selection index currently in the focus frame (fractional while moving)
static uint64_t scroll_last_us = 0;


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
            path_t *path = directory ? path_clone_push(directory, entry->name) : path_create(entry->name);
            char game_code[4];
            slot->has_id = labels_rom_id(path_get(path), &slot->id, game_code);
            if (slot->has_id) {
                slot->has_title = rom_info_metadata_title(path_get(path), game_code, slot->title, sizeof(slot->title));
            }
            path_free(path);
        }
        if (slot->has_id) {
            slot->small = load_label(slot->id, &small_style);
        }
        sound_poll();   // each new tile reads the SD card; keep audio flowing between them
    }

    if (large && !slot->large_loaded) {
        slot->large_loaded = true;
        if (slot->small) {
            slot->large = load_label(slot->id, &large_style);
            sound_poll();
        }
    }

    return large ? slot->large : slot->small;
}

/**
 * @brief Turn a file name like "Legend of Zelda, The - Majora's Mask (USA).z64"
 *        into a display title like "The Legend of Zelda - Majora's Mask".
 */
void ui_components_carousel_title (const char *name, bool directory, char *out, size_t out_size) {
    // Favorites and History pass full paths; only the file name is shown.
    const char *slash = strrchr(name, '/');
    snprintf(out, out_size, "%s", slash ? slash + 1 : name);

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

/**
 * @brief Folder shape filling the same w x h footprint as a cartridge: a tab on top-left and a body.
 *
 * @param grow Expands both parts outward (used to draw the selection outline behind the folder).
 */
static void draw_folder_shape (float x, float y, float w, float h, float grow, color_t color) {
    float tab_w = w * 0.4f;
    float tab_h = h * 0.16f;
    ui_components_box_draw(x - grow, y - grow, x + tab_w + grow, y + tab_h + grow, color);
    ui_components_box_draw(x - grow, y + (tab_h * 0.6f) - grow, x + w + grow, y + h + grow, color);
}

/**
 * @brief Draw a cartridge tile of any size between the small and large tiles.
 *
 * At its native size a tile is copied 1:1; while it is growing or shrinking (only during
 * scrolling) the nearer style is scaled with point sampling. Bilinear filtering would blend
 * the black of transparent pixels into the cartridge edges and show strip seams.
 */
static void draw_tile (path_t *directory, entry_t *entry, int32_t position, int32_t selected, float x, float y, float size, float cartridge_width, bool large) {
    tile_style_t *style = large ? &large_style : &small_style;
    float scale = cartridge_width / style->cartridge_width;
    bool native = (fabsf(scale - 1.0f) < 0.001f);

    if (native) {
        x = roundf(x);
        y = roundf(y);
    }

    float cx = x + (size - cartridge_width) / 2;
    float cy = y + (size - (style->cartridge_height * scale)) / 2;
    if (native) {
        cx = roundf(cx);
        cy = roundf(cy);
    }

    if (entry->type == ENTRY_TYPE_DIR) {
        draw_folder_shape(cx, cy, cartridge_width, style->cartridge_height * scale, 0, CAROUSEL_FOLDER_COLOR);
        return;
    }
    float lx = cx + (style->label_x * scale);
    float ly = cy + (style->label_y * scale);
    float bleed = CARTRIDGE_LABEL_BLEED * scale;
    surface_t *label = label_get(directory, entry, position, selected, large);

    rdpq_mode_push();
        if (native) {
            rdpq_set_mode_copy(true);
        } else {
            rdpq_set_mode_standard();
            rdpq_mode_alphacompare(1);
            rdpq_mode_filter(FILTER_POINT);
        }
        rdpq_blitparms_t parms = { .scale_x = scale, .scale_y = scale };

        // The label goes underneath; the cartridge is an overlay with a rounded window cut out for it.
        if (label) {
            rdpq_tex_blit(label, lx - bleed, ly - bleed, &parms);
        }
    rdpq_mode_pop();

    if (!label) {
        ui_components_box_draw(lx, ly, lx + (style->label_width * scale), ly + (style->label_height * scale), CAROUSEL_PLACEHOLDER_COLOR);
    }

    if (style->cartridge) {
        rdpq_mode_push();
            if (native) {
                rdpq_set_mode_copy(true);
            } else {
                rdpq_set_mode_standard();
                rdpq_mode_alphacompare(1);
                rdpq_mode_filter(FILTER_POINT);
            }
            rdpq_sprite_blit(style->cartridge, cx, cy, &parms);
        rdpq_mode_pop();
    }
}

/** @brief Display title for the entry at a list position: its metadata title if known, else from the file name. */
void ui_components_carousel_entry_title (entry_t *entry, int32_t position, char *out, size_t out_size) {
    for (int i = 0; i < LABEL_CACHE_SIZE; i++) {
        if (label_cache[i].position == position && label_cache[i].has_title) {
            snprintf(out, out_size, "%s", label_cache[i].title);
            return;
        }
    }
    ui_components_carousel_title(entry->name, entry->type == ENTRY_TYPE_DIR, out, out_size);
}

static void draw_tile_caption (entry_t *entry, int32_t position, float centre_x) {
    char title[128];
    ui_components_carousel_entry_title(entry, position, title, sizeof(title));
    for (char *c = title; *c; c++) {
        *c = toupper((unsigned char) (*c));
    }

    ui_components_text_draw(
        &(rdpq_textparms_t) {
            .style_id = STL_GRAY,
            .width = CAROUSEL_TILE_SIZE,
            .align = ALIGN_CENTER,
            .wrap = WRAP_ELLIPSES,
        },
        FNT_DEFAULT,    // PixelOperator 16px: fits more of the name under a small tile
        roundf(centre_x - (CAROUSEL_TILE_SIZE / 2)),
        CAROUSEL_CAPTION_Y,
        title
    );
}

/**
 * @brief Left edge of a tile that is `d` steps from the focus frame (d may be fractional).
 *
 * Whole steps give the resting layout: the large focused tile, a wider gap on each side of it,
 * then small tiles at a regular pitch. Fractional steps interpolate between neighbours.
 */
static float tile_x (float d) {
    float lo = floorf(d);
    float t = d - lo;
    float x0, x1;
    for (int k = 0; k < 2; k++) {
        int n = (int) (lo) + k;
        float x;
        if (n == 0) {
            x = CAROUSEL_SELECTED_X;
        } else if (n > 0) {
            x = CAROUSEL_SELECTED_X + CAROUSEL_SELECTED_TILE_SIZE + CAROUSEL_SELECTED_GAP + ((n - 1) * CAROUSEL_TILE_PITCH);
        } else {
            x = CAROUSEL_SELECTED_X - CAROUSEL_SELECTED_GAP - CAROUSEL_TILE_SIZE + ((n + 1) * CAROUSEL_TILE_PITCH);
        }
        if (k == 0) x0 = x; else x1 = x;
    }
    return x0 + ((x1 - x0) * t);
}

/** @brief Tile size `d` steps from the focus frame: large at 0, small from 1 step away. */
static float tile_size (float d) {
    float t = fminf(fabsf(d), 1.0f);
    return CAROUSEL_SELECTED_TILE_SIZE + ((CAROUSEL_TILE_SIZE - CAROUSEL_SELECTED_TILE_SIZE) * t);
}

/** @brief Move the scroll position toward the selection; returns true once it has arrived. */
static bool scroll_update (int32_t selected, int32_t entries) {
    uint64_t now = get_ticks_us();

    if (!scroll_ready) {
        scroll_ready = true;
        scroll_position = selected;
        scroll_last_us = now;
        return true;
    }

    float dt = (now - scroll_last_us) / 1000000.0f;
    scroll_last_us = now;

    float distance = selected - scroll_position;
    // Big jumps animate as a single step instead of sweeping the list.
    if (fabsf(distance) > CAROUSEL_MAX_ANIMATED_STEPS) {
        if (fabsf(distance) > entries / 2.0f) {
            // Wrapped around the end of the list: keep moving the way the player pressed, so the
            // new tile slides in from the side they are heading towards.
            scroll_position = selected + copysignf(1.0f, distance);
        } else {
            scroll_position = selected - copysignf(1.0f, distance);
        }
        distance = selected - scroll_position;
    }

    // Exponential ease-out, frame rate independent.
    scroll_position += distance * (1.0f - expf(-dt * CAROUSEL_SCROLL_SPEED));
    if (fabsf(selected - scroll_position) < 0.01f) {
        scroll_position = selected;
    }
    return scroll_position == selected;
}

/**
 * @brief Forget cached labels and snap the scroll position (call when the directory listing changes).
 */
void ui_components_carousel_invalidate (void) {
    label_cache_reset();
    scroll_ready = false;
}

/**
 * @brief Draw the carousel row and the selected entry's title.
 */
void ui_components_carousel_draw (path_t *directory, entry_t *list, int32_t entries, int32_t selected, bool selected_favorite) {
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
    if (selected < 0 || selected >= entries) {
        return;
    }

    bool settled = scroll_update(selected, entries);

    int32_t first = MAX(0, (int32_t) floorf(scroll_position) - 3);
    int32_t last = MIN(entries - 1, (int32_t) ceilf(scroll_position) + 5);
    for (int32_t i = first; i <= last; i++) {
        float d = i - scroll_position;
        float size = tile_size(d);
        float x = tile_x(d);
        if (x + size <= 0 || x >= DISPLAY_WIDTH) {
            continue;
        }
        float y = CAROUSEL_TILE_Y + ((CAROUSEL_SELECTED_TILE_SIZE - size) / 2);
        // Interpolate the cartridge width itself so switching sprites halfway through doesn't jump.
        float t = fminf(fabsf(d), 1.0f);
        float cartridge_width = CARTRIDGE_LARGE_WIDTH + ((CARTRIDGE_WIDTH - CARTRIDGE_LARGE_WIDTH) * t);
        bool focused = fabsf(d) < 0.5f;
        draw_tile(directory, &list[i], i, selected, x, y, size, cartridge_width, focused);
        if (!focused) {
            draw_tile_caption(&list[i], i, x + (size / 2));
        }
    }

    entry_t *entry = &list[selected];

    // Selection outline around the focused cartridge or folder, once it has finished moving into place.
    if (settled) {
        int cx = CAROUSEL_SELECTED_X + ((CAROUSEL_SELECTED_TILE_SIZE - CARTRIDGE_LARGE_WIDTH) / 2);
        int cy = CAROUSEL_TILE_Y + ((CAROUSEL_SELECTED_TILE_SIZE - CARTRIDGE_LARGE_HEIGHT) / 2);
        if (entry->type == ENTRY_TYPE_DIR) {
            // White folder grown by the outline width, then the folder again on top of it.
            draw_folder_shape(cx, cy, CARTRIDGE_LARGE_WIDTH, CARTRIDGE_LARGE_HEIGHT, CARTRIDGE_OUTLINE_OFFSET, CAROUSEL_OUTLINE_COLOR);
            draw_folder_shape(cx, cy, CARTRIDGE_LARGE_WIDTH, CARTRIDGE_LARGE_HEIGHT, 0, CAROUSEL_FOLDER_COLOR);
        } else {
            if (!selection_outline) {
                selection_outline = sprite_load("rom:/cartridge_large_outline.sprite");
                favorite_outline = sprite_load("rom:/cartridge_large_outline_favorite.sprite");
                hidden_outline = sprite_load("rom:/cartridge_large_outline_hidden.sprite");
            }
            rdpq_mode_push();
                rdpq_set_mode_copy(true);
                // Hidden (light red) wins over favorite (gold): it's the state you'd want to notice.
                sprite_t *outline = entry->hidden ? hidden_outline : (selected_favorite ? favorite_outline : selection_outline);
                rdpq_sprite_blit(outline, cx - CARTRIDGE_OUTLINE_OFFSET, cy - CARTRIDGE_OUTLINE_OFFSET, NULL);
            rdpq_mode_pop();
        }
    }

    char title[128];
    ui_components_carousel_entry_title(entry, selected, title, sizeof(title));

    ui_components_text_draw(
        &(rdpq_textparms_t) {
            .style_id = entry->hidden ? STL_GRAY : STL_DEFAULT,     // hidden games, when shown, are grayed out
        },  // no width: long titles run off the edge of the screen
        FNT_TITLE,
        GAME_INFO_TITLE_X,
        CAROUSEL_TITLE_Y,
        title
    );
}
