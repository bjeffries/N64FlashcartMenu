/**
 * @file history_favorites.c
 * @brief Favorites and History tabs: the Library carousel over bookkeeping entries
 * @ingroup view
 */

#include <string.h>

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

static void entries_load (void) {
    entries_free();
    for (uint16_t i = 0; i < item_max; i++) {
        bookkeeping_item_t *item = &item_list[i];
        if (item->bookkeeping_type == BOOKKEEPING_TYPE_EMPTY || !path_has_value(item->primary_path)) {
            continue;
        }
        entries[entry_count++] = (entry_t) {
            .name = strdup(path_get(item->primary_path)),
            .type = (item->bookkeeping_type == BOOKKEEPING_TYPE_DISK) ? ENTRY_TYPE_DISK : ENTRY_TYPE_ROM,
            .size = 0,
            .index = i,
        };
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

    if (entry->type == ENTRY_TYPE_DISK) {
        menu->next_mode = MENU_MODE_LOAD_DISK;
    } else {
        menu->load.play_now = !configure;
        menu->load.open_configure = configure;
        menu->next_mode = MENU_MODE_LOAD_ROM;
    }
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
        if (menu->actions.go_left && selected > 0) {
            selected--;
            sound_play_effect(SFX_CURSOR);
        } else if (menu->actions.go_right && selected < entry_count - 1) {
            selected++;
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
    } else if (menu->actions.remove && tab == TAB_FAVORITES) {
        bookkeeping_favorite_remove(&menu->bookkeeping, entries[selected].index);
        entries_load();
        sound_play_effect(SFX_SETTING);
    } else if ((menu->actions.go_up || menu->actions.go_down) && !menu->actions.go_fast) {
        int pages = ui_components_game_info_page_count(&entries[selected]);
        if (pages > 1) {
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
                ? "No favorites yet.\nPress C-Right on a game and choose\n\"Add to favorites\"."
                : "Nothing played yet.\nGames you launch will appear here."
        );
        rdpq_detach_show();
        return;
    }

    ui_components_carousel_draw(NULL, entries, entry_count, selected);

    int pages = ui_components_game_info_page_count(&entries[selected]);
    if (info_page >= pages) {
        info_page = 0;
    }
    ui_components_game_info_draw(NULL, &entries[selected], &menu->bookkeeping, info_page);
    ui_components_game_info_dots_draw(info_page, pages);

    int x = GAME_INFO_VALUE_X;
    x += ui_components_button_hint_draw(ICON_A, x, LIBRARY_BUTTONS_Y, "Play Cartridge") + LIBRARY_HINT_GAP;
    if (entries[selected].type == ENTRY_TYPE_ROM) {
        x += ui_components_button_hint_draw(ICON_C_RIGHT, x, LIBRARY_BUTTONS_Y, "Configure") + LIBRARY_HINT_GAP;
    }
    if (tab == TAB_FAVORITES) {
        ui_components_button_hint_draw(ICON_C_UP, x, LIBRARY_BUTTONS_Y, "Remove");
    }

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
