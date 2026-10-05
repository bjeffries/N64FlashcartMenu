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

/*
 * Cartridge labels are read from labels.db in the background, never while a frame waits on them:
 * each frame the carousel is still, the labels nearest the selection that aren't cached yet are
 * loaded within LABEL_LOAD_BUDGET_US (at least one); a cartridge whose label isn't loaded yet shows
 * the placeholder. The label key (a checksum of the ROM's first 8KB) comes from the info cache,
 * which reads it during the boot animation and while idle. Each label is read once and kept at
 * both sizes. Labels are kept per game (by path), so they survive changing folder. When the cache
 * is full, the label used longest ago is replaced; a label counts as used when it's drawn, or while
 * it's within the window kept loaded around the selection.
 */
#define LABEL_CACHE_SIZE_EXPANDED   (128)   // ~18KB a game: ~2.3MB with an Expansion Pak
#define LABEL_CACHE_SIZE_4MB        (24)    // ~430KB without one
#define LABEL_LOAD_BUDGET_US        (6000)

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
    uint32_t game;          // hash of the game's path, 0 when the slot is free
    int32_t position;       // its position in the list on screen, -1 if it isn't in it (or not looked up yet)
    uint32_t last_used;     // use_clock when it was last drawn or in the kept window
    surface_t *small;       // NULL when there is no label
    surface_t *large;
} label_slot_t;

static label_slot_t *label_cache = NULL;
static int label_cache_size = 0;
static bool label_cache_ready = false;

// The list being drawn, for loading labels and titles.
static path_t *list_directory;  // NULL: entry names are full paths (Favorites)
static entry_t *list_entries;
static int32_t list_count;
static bool skip_label_loading = false;     // for one draw: a game is loading behind it
static uint32_t list_signature = 0;         // which list the cached labels belong to
static uint32_t use_clock = 0;              // counts carousel draws, for last_used
static bool labels_pending = false;         // labels near the selection still to load

static sprite_t *selection_outline = NULL;
static sprite_t *favorite_outline = NULL;
static sprite_t *hidden_outline = NULL;

static bool scroll_ready = false;       // false: snap to the selection on the next draw
static float scroll_position = 0;       // selection index currently in the focus frame (fractional while moving)
static uint64_t scroll_last_us = 0;
static uint32_t last_moving_ms = 0;    // last draw where the row was still sliding


static void label_cache_reset (void) {
    if (!label_cache) {
        label_cache_size = is_memory_expanded() ? LABEL_CACHE_SIZE_EXPANDED : LABEL_CACHE_SIZE_4MB;
        label_cache = calloc(label_cache_size, sizeof(label_slot_t));
        if (!label_cache) {
            label_cache_size = 0;
        }
    }
    for (int i = 0; i < label_cache_size; i++) {
        if (label_cache_ready) {
            labels_free(label_cache[i].small);
            labels_free(label_cache[i].large);
        }
        label_cache[i] = (label_slot_t) { .position = -1 };
    }
    label_cache_ready = true;
    list_signature = 0;
}

static uint32_t hash_text (uint32_t hash, const char *text) {
    for (const char *c = text; *c; c++) {
        hash = (hash ^ (uint8_t) (*c)) * 16777619u;     // FNV-1a
    }
    return hash;
}

/** @brief Hash of an entry's full path (the same for a game in the Library and in Favorites). */
static uint32_t hash_game_path (path_t *directory, const char *name) {
    uint32_t hash = 2166136261u;
    if (directory) {
        const char *dir = path_get(directory);
        hash = hash_text(hash, dir);
        size_t length = strlen(dir);
        if (length == 0 || dir[length - 1] != '/') {
            hash = hash_text(hash, "/");
        }
    }
    hash = hash_text(hash, name);
    return hash ? hash : 1;
}

static uint32_t game_hash (int32_t position) {
    return hash_game_path(list_directory, list_entries[position].name);
}

/** @brief Draw / load from this list; the cached labels are dropped if they belong to another one. */
static void use_list (path_t *directory, entry_t *list, int32_t entries) {
    if (!label_cache_ready) {
        label_cache_reset();
    }
    uint32_t signature = hash_text(2166136261u, directory ? path_get(directory) : "");
    signature = (signature ^ (uint32_t) entries) * 16777619u;
    if (entries > 0) {
        signature = hash_text(hash_text(signature, list[0].name), list[entries - 1].name);
    }
    signature |= 1;     // never 0 (0: none)
    if (signature != list_signature) {
        // Another list: keep the labels, but their positions are looked up again.
        for (int i = 0; i < label_cache_size; i++) {
            label_cache[i].position = -1;
        }
        list_signature = signature;
    }
    list_directory = directory;
    list_entries = list;
    list_count = entries;
}

static label_slot_t *label_find (int32_t position) {
    for (int i = 0; i < label_cache_size; i++) {
        if (label_cache[i].position == position) {
            return &label_cache[i];
        }
    }
    if (list_entries[position].type != ENTRY_TYPE_ROM) {
        return NULL;
    }
    // Not looked up in this list yet: a label kept from another folder?
    uint32_t game = game_hash(position);
    for (int i = 0; i < label_cache_size; i++) {
        if (label_cache[i].game == game) {
            label_cache[i].position = position;
            return &label_cache[i];
        }
    }
    return NULL;
}

/** @brief A cached label, or NULL (not loaded yet, or the game has none); counts as a use. */
static surface_t *label_get (int32_t position, bool large) {
    label_slot_t *slot = label_find(position);
    if (!slot) {
        return NULL;
    }
    slot->last_used = use_clock;
    return large ? slot->large : slot->small;
}

/** @brief The slot to load a new label into: a free one, else the one used longest ago. */
static label_slot_t *label_slot_to_replace (void) {
    label_slot_t *oldest = NULL;
    for (int i = 0; i < label_cache_size; i++) {
        label_slot_t *slot = &label_cache[i];
        if (slot->game == 0) {
            return slot;
        }
        if (!oldest || (int32_t) (use_clock - slot->last_used) > (int32_t) (use_clock - oldest->last_used)) {
            oldest = slot;
        }
    }
    return oldest;
}

/** @brief Read the label for a list position into a slot, at both sizes. */
static void label_read (label_slot_t *slot, int32_t position) {
    *slot = (label_slot_t) { .game = game_hash(position), .position = position, .last_used = use_clock };
    entry_t *entry = &list_entries[position];
    path_t *path = list_directory ? path_clone_push(list_directory, entry->name) : path_create(entry->name);
    uint32_t id;
    int known = ui_components_game_info_label_id(path_get(path), &id);
    bool has_id = (known == 1) || ((known < 0) && labels_rom_id(path_get(path), &id, NULL));
    path_free(path);
    if (has_id) {
        // Slightly larger than the window, so the label's printed edge hides under the cartridge.
        labels_load_pair(id,
            large_style.label_width + (CARTRIDGE_LABEL_BLEED * 2), large_style.label_height + (CARTRIDGE_LABEL_BLEED * 2), &slot->large,
            small_style.label_width + (CARTRIDGE_LABEL_BLEED * 2), small_style.label_height + (CARTRIDGE_LABEL_BLEED * 2), &slot->small);
    }
    sound_poll();
}

/** @brief Load the uncached labels nearest the selection, for up to budget_us (at least one). */
static void labels_update (int32_t selected, uint32_t budget_us) {
    labels_pending = false;
    if (label_cache_size == 0 || list_count == 0) {
        return;
    }
    uint64_t start = get_ticks_us();
    int32_t reach = MIN((label_cache_size / 2) - 1, list_count / 2);

    // The window kept around the selection (smaller than the cache) counts as in use, so filling
    // one end of it never pushes out the other.
    for (int32_t offset = -reach; offset <= reach; offset++) {
        int32_t position = (selected + offset + list_count) % list_count;
        label_slot_t *slot = (list_entries[position].type == ENTRY_TYPE_ROM) ? label_find(position) : NULL;
        if (slot) {
            slot->last_used = use_clock;
        }
    }

    for (int32_t d = 0; d <= reach; d++) {
        for (int side = 0; side < 2; side++) {
            if (d == 0 && side == 1) {
                continue;
            }
            int32_t position = (selected + (side ? -d : d) + list_count) % list_count;
            if (list_entries[position].type != ENTRY_TYPE_ROM || label_find(position)) {
                continue;
            }
            label_slot_t *slot = label_slot_to_replace();
            if (!slot || (slot->game != 0 && slot->last_used == use_clock)) {
                return;     // everything cached is in use right now
            }
            labels_free(slot->small);
            labels_free(slot->large);
            label_read(slot, position);
            if ((get_ticks_us() - start) >= budget_us) {
                labels_pending = true;      // there may be more
                return;
            }
        }
    }
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
    surface_t *label = label_get(position, large);

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
    if (entry->type == ENTRY_TYPE_ROM) {
        // Metadata titles come from the info cache.
        path_t *path = list_directory ? path_clone_push(list_directory, entry->name) : path_create(entry->name);
        const char *title = ui_components_game_info_cached_title(path_get(path));
        if (title) {
            snprintf(out, out_size, "%s", title);
        }
        path_free(path);
        if (title) {
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

    ui_components_body_text_draw(
        &(rdpq_textparms_t) {
            .style_id = STL_GRAY,
            .width = CAROUSEL_TILE_SIZE,
            .align = ALIGN_CENTER,
            .wrap = WRAP_ELLIPSES,
        },
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

surface_t *ui_components_carousel_cached_label (path_t *directory, const char *name) {
    uint32_t game = hash_game_path(directory, name);
    for (int i = 0; i < label_cache_size; i++) {
        if (label_cache[i].game == game) {
            return label_cache[i].large;
        }
    }
    return NULL;
}

surface_t *ui_components_cartridge_label_load (path_t *directory, const char *name) {
    path_t *path = directory ? path_clone_push(directory, (char *) name) : path_create(name);
    uint32_t id;
    int known = ui_components_game_info_label_id(path_get(path), &id);
    bool has_id = (known == 1) || ((known < 0) && labels_rom_id(path_get(path), &id, NULL));
    path_free(path);
    surface_t *large = NULL;
    if (has_id) {
        labels_load_pair(id,
            large_style.label_width + (CARTRIDGE_LABEL_BLEED * 2), large_style.label_height + (CARTRIDGE_LABEL_BLEED * 2), &large,
            0, 0, NULL);
    }
    sound_poll();
    return large;
}

void ui_components_cartridge_draw (float cx, float cy, float scale, surface_t *label, uint8_t label_brightness, uint8_t brightness) {
    if (!large_style.cartridge) {
        large_style.cartridge = sprite_load("rom:/cartridge_large.sprite");
    }
    float x = cx - ((CARTRIDGE_LARGE_WIDTH * scale) / 2.0f);
    float y = cy - ((CARTRIDGE_LARGE_HEIGHT * scale) / 2.0f);
    float lx = x + (CARTRIDGE_LARGE_LABEL_X * scale);
    float ly = y + (CARTRIDGE_LARGE_LABEL_Y * scale);
    float bleed = CARTRIDGE_LABEL_BLEED * scale;
    rdpq_blitparms_t parms = { .scale_x = scale, .scale_y = scale };

    if (!label) {
        color_t placeholder = CAROUSEL_PLACEHOLDER_COLOR;
        placeholder.r = (placeholder.r * label_brightness) / 0xFF;
        placeholder.g = (placeholder.g * label_brightness) / 0xFF;
        placeholder.b = (placeholder.b * label_brightness) / 0xFF;
        ui_components_box_draw(lx, ly, lx + (CARTRIDGE_LARGE_LABEL_WIDTH * scale), ly + (CARTRIDGE_LARGE_LABEL_HEIGHT * scale), placeholder);
    }
    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_alphacompare(1);
        rdpq_mode_filter(FILTER_POINT);
        // Darkened as they're drawn (texel colour times a grey), in the same single pass.
        bool shaded = (label_brightness < 0xFF) || (brightness < 0xFF);
        if (shaded) {
            rdpq_mode_combiner(RDPQ_COMBINER1((TEX0, 0, PRIM, 0), (0, 0, 0, TEX0)));
        }
        // The label goes underneath; the cartridge is an overlay with a window cut out for it.
        if (label) {
            if (shaded) {
                rdpq_set_prim_color(RGBA32(label_brightness, label_brightness, label_brightness, 0xFF));
            }
            rdpq_tex_blit(label, lx - bleed, ly - bleed, &parms);
        }
        if (shaded) {
            rdpq_set_prim_color(RGBA32(brightness, brightness, brightness, 0xFF));
        }
        rdpq_sprite_blit(large_style.cartridge, x, y, &parms);
    rdpq_mode_pop();
}

bool ui_components_carousel_labels_pending (void) {
    return labels_pending;
}

void ui_components_carousel_snap (void) {
    scroll_ready = false;
}

void ui_components_carousel_preload (path_t *directory, entry_t *list, int32_t entries, int32_t selected, uint32_t budget_us) {
    use_list(directory, list, entries);
    use_clock++;
    if (selected >= 0 && selected < entries) {
        labels_update(selected, budget_us);
    }
}

uint32_t ui_components_carousel_still_ms (void) {
    return get_ticks_ms() - last_moving_ms;
}

/**
 * @brief Draw the carousel row and the selected entry's title.
 */
void ui_components_carousel_draw (path_t *directory, entry_t *list, int32_t entries, int32_t selected, bool selected_favorite) {
    use_list(directory, list, entries);
    use_clock++;
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
    if (!settled) {
        last_moving_ms = get_ticks_ms();
    }

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

    // Labels load only while the row is still (and not while a game loads behind it).
    if (settled && !skip_label_loading) {
        labels_update(selected, LABEL_LOAD_BUDGET_US);
    }
    skip_label_loading = false;
}

void ui_components_carousel_skip_label_loading (void) {
    skip_label_loading = true;
}
