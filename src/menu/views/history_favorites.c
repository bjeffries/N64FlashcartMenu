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

    // Favorites are alphabetical, so a long hold pages by letter; History is by date.
    selected = ui_components_carousel_scroll(menu, entries, entry_count, selected, tab == TAB_FAVORITES);

    if (entry_count == 0) {
        return;
    }

    if (menu->actions.enter) {
        sound_play_effect(SFX_ENTER);
        open_selected(menu, false);
    } else if (menu->actions.c_up && entries[selected].type == ENTRY_TYPE_ROM) {    // Config
        sound_play_effect(SFX_SETTING);
        open_selected(menu, true);
    } else if (menu->actions.c_left) {    // Favorite / Unfavorite
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

/**
 * @brief The whole screen; show_hints is false while a game loads,
 *        and then only the tabs, carousel and title are drawn.
 */
static void draw_content (menu_t *menu, bool show_hints) {
    ui_components_tab_header_draw(tab);

    if (entry_count == 0) {
        ui_components_body_text_draw(
            &(rdpq_textparms_t) { .style_id = STL_GRAY, .width = VISIBLE_AREA_X1 - CAROUSEL_SELECTED_X, .wrap = WRAP_WORD },
            CAROUSEL_SELECTED_X, CAROUSEL_TILE_Y + 40,
            (tab == TAB_FAVORITES)
                ? "No favorites yet.\nPress C-Left on a game in the Library\nto add it here."
                : "Nothing played yet.\nGames you launch will appear here."
        );
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
    // While a game loads, the info panel (and its page dots) is left black for the animation.
    if (!show_hints) {
        return;
    }
    ui_components_game_info_draw(NULL, &entries[selected], &menu->bookkeeping, info_page);
    ui_components_game_info_dots_draw(info_page, pages);
    ui_components_letter_indicator_draw();
    if (tab == TAB_FAVORITES) {
        ui_components_position_indicator_draw(selected + 1, entry_count);
    }

    button_hint_t hints[BUTTON_HINTS_MAX];
    int count = 0;
    hints[count++] = (button_hint_t) { ICON_A, "Play" };
    hints[count++] = (button_hint_t) { ICON_C_LEFT, favorite ? "Unfave" : "Fave" };
    if (entries[selected].type == ENTRY_TYPE_ROM) {
        hints[count++] = (button_hint_t) { ICON_C_UP, "Config" };
    }
    ui_components_button_hints_draw(hints, count);
}

static void draw (menu_t *menu, surface_t *display) {
    ui_components_attach_clear(display);
    draw_content(menu, true);
    rdpq_detach_show();
}

/**
 * @brief Draw Favorites / History as the background of the game loading animation (attached
 *        surface): tabs, carousel and title, with the info panel and button hints left black.
 */
void view_history_favorites_draw_behind_loading (menu_t *menu) {
    draw_content(menu, false);
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
    ui_components_carousel_scroll_reset();
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
