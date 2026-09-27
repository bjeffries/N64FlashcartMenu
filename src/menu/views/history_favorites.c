/**
 * @file history_favorites.c
 * @brief Favorites and History tabs: the Library carousel over bookkeeping entries
 * @ingroup view
 */

#include <string.h>
#include <strings.h>

#include "../bookkeeping.h"
#include "../fonts.h"
#include "../ui_components/constants.h"
#include "../sound.h"
#include "views.h"


static menu_tab_t tab;
static bookkeeping_item_t *item_list;
static uint16_t item_max;

// Carousel entries for the non-empty bookkeeping slots; entry.index is the slot, entry.name the full path.
static entry_t entries[HISTORY_COUNT > FAVORITES_COUNT ? HISTORY_COUNT : FAVORITES_COUNT];
static int32_t entry_count = 0;
static int32_t selected = 0;
static int info_page = 0;
static int hold_frames = 0;


static void entries_free (void) {
    for (int32_t i = 0; i < entry_count; i++) {
        free(entries[i].name);
    }
    entry_count = 0;
    ui_components_carousel_invalidate();
}

/** @brief Order by file name, like the Library (names are full paths, so compare after the last '/'). */
static int compare_file_names (const void *a, const void *b) {
    const char *name_a = ((const entry_t *) (a))->name;
    const char *name_b = ((const entry_t *) (b))->name;
    const char *slash_a = strrchr(name_a, '/');
    const char *slash_b = strrchr(name_b, '/');
    return strcasecmp(slash_a ? slash_a + 1 : name_a, slash_b ? slash_b + 1 : name_b);
}

static void entries_load (void) {
    entries_free();
    for (uint16_t i = 0; i < item_max; i++) {
        bookkeeping_item_t *item = &item_list[i];
        // Only ROMs; entries from older versions of the menu may still include 64DD disks.
        if (item->bookkeeping_type != BOOKKEEPING_TYPE_ROM || !path_has_value(item->primary_path)) {
            continue;
        }
        entries[entry_count++] = (entry_t) {
            .name = strdup(path_get(item->primary_path)),
            .type = ENTRY_TYPE_ROM,
            .size = 0,
            .index = i,
        };
    }
    // Favorites are alphabetical; History stays most recent first.
    if (tab == TAB_FAVORITES) {
        qsort(entries, entry_count, sizeof(entry_t), compare_file_names);
    }
    if (selected >= entry_count) {
        selected = entry_count > 0 ? entry_count - 1 : 0;
    }
}

static void open_selected (menu_t *menu, bool configure) {
    entry_t *entry = &entries[selected];

    menu->load.load_history_id = (tab == TAB_HISTORY) ? entry->index : -1;
    menu->load.load_favorite_id = (tab == TAB_FAVORITES) ? entry->index : -1;
    menu->load.return_mode = (tab == TAB_HISTORY) ? MENU_MODE_HISTORY : MENU_MODE_FAVORITE;

    menu->load.play_now = !configure;
    menu->load.open_configure = configure;
    menu->next_mode = MENU_MODE_LOAD_ROM;
}

static void process (menu_t *menu) {
    if (ui_components_tab_process(menu, tab)) {
        return;
    }

    // Held directions repeat every frame; throttle that to one tile every CAROUSEL_REPEAT_FRAMES.
    bool horizontal = (menu->actions.go_left || menu->actions.go_right) && !menu->actions.go_fast;
    bool move_now = horizontal && (hold_frames % CAROUSEL_REPEAT_FRAMES == 0);
    hold_frames = horizontal ? hold_frames + 1 : 0;

    if (entry_count > 1 && move_now) {
        if (menu->actions.go_left) {
            selected = (selected + entry_count - 1) % entry_count;
            sound_play_effect(SFX_CURSOR);
        } else if (menu->actions.go_right) {
            selected = (selected + 1) % entry_count;
            sound_play_effect(SFX_CURSOR);
        }
    }

    if (entry_count == 0) {
        return;
    }

    if (menu->actions.enter) {
        sound_play_effect(SFX_ENTER);
        open_selected(menu, false);
    } else if (menu->actions.configure && entries[selected].type == ENTRY_TYPE_ROM) {
        sound_play_effect(SFX_SETTING);
        open_selected(menu, true);
    } else if (menu->actions.favorite) {
        path_t *path = path_create(entries[selected].name);
        int slot = bookkeeping_favorite_find(&menu->bookkeeping, path);
        if (slot >= 0) {
            bookkeeping_favorite_remove(&menu->bookkeeping, slot);
        } else {
            bookkeeping_favorite_add(&menu->bookkeeping, path, NULL, BOOKKEEPING_TYPE_ROM);
        }
        path_free(path);
        if (tab == TAB_FAVORITES) {
            entries_load();     // the game just left the Favorites list
        }
        sound_play_effect(SFX_SETTING);
    } else if ((menu->actions.go_up || menu->actions.go_down) && !menu->actions.go_fast) {
        int pages = ui_components_game_info_page_count(&entries[selected]);
        // On the About page, up / down scroll the description first and change page at its ends.
        if (ui_components_game_info_scroll(menu->actions.go_down ? 1 : -1)) {
            sound_play_effect(SFX_CURSOR);
        } else if (pages > 1) {
            info_page = (info_page + (menu->actions.go_down ? 1 : pages - 1)) % pages;
            sound_play_effect(SFX_CURSOR);
        }
    }
}

static void draw (menu_t *menu, surface_t *display) {
    rdpq_attach_clear(display, NULL);

    ui_components_tab_header_draw(tab);

    if (entry_count == 0) {
        ui_components_text_draw(
            &(rdpq_textparms_t) { .style_id = STL_GRAY, .width = VISIBLE_AREA_X1 - CAROUSEL_SELECTED_X, .wrap = WRAP_WORD },
            FNT_DEFAULT, CAROUSEL_SELECTED_X, CAROUSEL_TILE_Y + 40,
            (tab == TAB_FAVORITES)
                ? "No favorites yet.\nPress C-Left on a game in the Library\nto add it here."
                : "Nothing played yet.\nGames you launch will appear here."
        );
        rdpq_detach_show();
        return;
    }

    path_t *path = path_create(entries[selected].name);
    bool favorite = bookkeeping_favorite_find(&menu->bookkeeping, path) >= 0;
    path_free(path);

    ui_components_carousel_draw(NULL, entries, entry_count, selected, favorite);

    int pages = ui_components_game_info_page_count(&entries[selected]);
    if (info_page >= pages) {
        info_page = 0;
    }
    ui_components_game_info_draw(NULL, &entries[selected], &menu->bookkeeping, info_page);
    ui_components_game_info_dots_draw(info_page, pages);

    int x = GAME_INFO_VALUE_X;
    x += ui_components_button_hint_draw(ICON_A, x, LIBRARY_BUTTONS_Y, "Play") + LIBRARY_HINT_GAP;
    if (entries[selected].type == ENTRY_TYPE_ROM) {
        x += ui_components_button_hint_draw(ICON_C_RIGHT, x, LIBRARY_BUTTONS_Y, "Config") + LIBRARY_HINT_GAP;
    }
    ui_components_button_hint_draw(ICON_C_LEFT, x, LIBRARY_BUTTONS_Y, favorite ? "Unfavorite" : "Favorite");

    rdpq_detach_show();
}

static void init (menu_t *menu, menu_tab_t new_tab) {
    if (tab != new_tab) {
        selected = 0;
        info_page = 0;
    }
    tab = new_tab;
    if (tab == TAB_FAVORITES) {
        item_list = menu->bookkeeping.favorite_items;
        item_max = FAVORITES_COUNT;
    } else {
        item_list = menu->bookkeeping.history_items;
        item_max = HISTORY_COUNT;
    }
    hold_frames = 0;
    entries_load();
}

void view_favorite_init (menu_t *menu) {
    init(menu, TAB_FAVORITES);
}

void view_favorite_display (menu_t *menu, surface_t *display) {
    process(menu);
    draw(menu, display);
}

void view_history_init (menu_t *menu) {
    init(menu, TAB_HISTORY);
}

void view_history_display (menu_t *menu, surface_t *display) {
    process(menu);
    draw(menu, display);
}
