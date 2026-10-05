/**
 * @file history.c
 * @brief History tab: every game in the Start Folder as a table of title, play count and last played
 * @ingroup view
 *
 * Laid out like the Cheat Codes table, 64 games to a page: up / down move through a page, C-Left /
 * C-Right change page, left / right choose the column to sort by and C-Up reverses the order.
 * A or Start plays the selected game.
 *
 * The games are found by searching the Start Folder (Menu Settings > Start Folder, /N64 by default)
 * and the folders in it (as many folders as fit in SCAN_BUDGET_US each frame, depth first, at most
 * SCAN_MAX_DEPTH folders below it) the first time the tab is opened, skipping the menu's own folder
 * and system folders. Titles start out from the file names; metadata titles replace them as the info
 * cache reads each game in the background (ui_components_game_info_prefetch).
 */

#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "../cart_load.h"
#include "../fonts.h"
#include "../hidden.h"
#include "../play_stats.h"
#include "../sound.h"
#include "../ui_components/constants.h"
#include "utils/fs.h"
#include "utils/utils.h"
#include "views.h"

#define VISIBLE_ROWS        (8)
#define PAGE_ROWS           (64)
#define NUMBER_X            (CAROUSEL_SELECTED_X + 16)
#define TITLE_X             (CAROUSEL_SELECTED_X + 56)    // room for 4-digit row numbers
#define LAST_PLAYED_X       (TABLE_SCROLLBAR_X - 168)
#define PLAY_COUNT_X        (LAST_PLAYED_X - 100)
#define COLUMN_GAP          (12)
#define TITLE_CHECKS        (64)    // games checked per frame for a newly cached metadata title
#define TABLE_FADE_MS       (300)   // the table fades out this fast when a game starts loading
#define SCAN_BUDGET_US      (8000)  // time per frame spent searching folders for games
#define SCAN_MAX_DEPTH      (3)     // folder levels searched below the Start Folder (/N64/Sports/Golf/Retro/Game.z64)

typedef enum {
    COLUMN_TITLE,
    COLUMN_PLAYS,
    COLUMN_LAST_PLAYED,
    COLUMN_COUNT,
} column_t;

static const char *column_names[COLUMN_COUNT] = { "TITLE", "PLAY COUNT", "LAST PLAYED" };

typedef struct {
    char *path;                 // full path ("sd:/N64/Game.z64")
    char *title;                // metadata title once known, else from the file name
    bool title_final;           // metadata title looked up
    uint32_t plays;
    time_t last_played;
} game_t;

static game_t *games;
static entry_t *entries;        // the same games for the info cache (name: full path)
static int *order;              // games in table order
static int game_count;
static int game_capacity;

static column_t sort_column = COLUMN_LAST_PLAYED;
static bool descending = true;
static int page;
static int selected;            // row on the page
static int first_visible;

static struct {
    bool started;
    bool done;
    bool show_hidden;           // settings it searched with: a change means searching again
    char *folder;               // the Start Folder
    int folders;                // folders searched so far
    path_t **stack;             // folders still to search...
    int *levels;                // ...and how deep each is (the root is 0)
    int depth;
    int capacity;
} scan;

static struct {
    bool fetched;               // the info cache has read every game
    bool done;                  // and the titles it found are in the table
    int cursor;
} titles;

static uint32_t loading_started_ms;


static int page_count (void) {
    return (game_count + PAGE_ROWS - 1) / PAGE_ROWS;
}

static int rows_on_page (void) {
    return MIN(PAGE_ROWS, game_count - (page * PAGE_ROWS));
}

static game_t *selected_game (void) {
    int index = (page * PAGE_ROWS) + selected;
    return (index < game_count) ? &games[order[index]] : NULL;
}


static void games_free (void) {
    for (int i = 0; i < game_count; i++) {
        free(games[i].path);
        free(games[i].title);
    }
    free(games);
    free(entries);
    free(order);
    games = NULL;
    entries = NULL;
    order = NULL;
    game_count = 0;
    game_capacity = 0;
    memset(&titles, 0, sizeof(titles));
}

static void add_game (path_t *path) {
    if (game_count == game_capacity) {
        int capacity = game_capacity ? game_capacity * 2 : 128;
        game_t *grown = realloc(games, capacity * sizeof(game_t));
        if (!grown) {
            return;
        }
        games = grown;
        game_capacity = capacity;
    }
    char title[128];
    ui_components_carousel_title(path_last_get(path), false, title, sizeof(title));
    game_t *game = &games[game_count];
    game->path = strdup(path_get(path));
    game->title = strdup(title);
    if (!game->path || !game->title) {
        free(game->path);
        free(game->title);
        return;
    }
    game->title_final = false;
    game->plays = play_stats_count(game->path);
    game->last_played = play_stats_last_played(game->path);
    game_count++;
}

static void scan_push (path_t *path, int level) {
    if (scan.depth == scan.capacity) {
        int capacity = scan.capacity ? scan.capacity * 2 : 16;
        path_t **grown = realloc(scan.stack, capacity * sizeof(path_t *));
        if (grown) {
            scan.stack = grown;
        }
        int *grown_levels = realloc(scan.levels, capacity * sizeof(int));
        if (grown_levels) {
            scan.levels = grown_levels;
        }
        if (!grown || !grown_levels) {
            path_free(path);
            return;
        }
        scan.capacity = capacity;
    }
    scan.levels[scan.depth] = level;
    scan.stack[scan.depth++] = path;
}

static void scan_free (void) {
    while (scan.depth > 0) {
        path_free(scan.stack[--scan.depth]);
    }
    free(scan.stack);
    free(scan.levels);
    free(scan.folder);
    memset(&scan, 0, sizeof(scan));
}

/** @brief Menu Settings > Start Folder: where the search starts. */
static const char *start_folder (menu_t *menu) {
    const char *folder = menu->settings.default_directory;
    return (folder && folder[0] != '\0') ? folder : "/";
}

static void scan_start (menu_t *menu) {
    scan_free();
    games_free();
    scan.started = true;
    scan.show_hidden = menu->settings.show_hidden_games;
    scan.folder = strdup(start_folder(menu));
    scan_push(path_init(menu->storage_prefix, scan.folder), 0);
}

static int compare_games (const void *a, const void *b) {
    const game_t *ga = &games[*(const int *) a];
    const game_t *gb = &games[*(const int *) b];
    int result = 0;
    switch (sort_column) {
        case COLUMN_TITLE:
            result = strcasecmp(ga->title, gb->title);
            break;
        case COLUMN_PLAYS:
            result = (ga->plays > gb->plays) - (ga->plays < gb->plays);
            break;
        case COLUMN_LAST_PLAYED:
            // Never played: always at the end.
            if ((ga->last_played == 0) != (gb->last_played == 0)) {
                return (ga->last_played == 0) ? 1 : -1;
            }
            result = (ga->last_played > gb->last_played) - (ga->last_played < gb->last_played);
            break;
        default:
            break;
    }
    if (descending) {
        result = -result;
    }
    return (result != 0) ? result : strcasecmp(ga->title, gb->title);
}

/** @brief Sort the table; keep_selection: stay on the same game, else go back to the top. */
static void sort_games (bool keep_selection) {
    if (game_count == 0) {
        return;
    }
    game_t *keep = keep_selection ? selected_game() : NULL;
    qsort(order, game_count, sizeof(int), compare_games);
    page = 0;
    selected = 0;
    first_visible = 0;
    for (int i = 0; keep && i < game_count; i++) {
        if (&games[order[i]] == keep) {
            page = i / PAGE_ROWS;
            selected = i % PAGE_ROWS;
            break;
        }
    }
}

/** @brief Search one folder: its games join the table, its folders are searched later. */
static void scan_step (menu_t *menu) {
    if (scan.depth == 0) {
        scan.done = true;
        order = malloc(MAX(1, game_count) * sizeof(int));
        entries = calloc(MAX(1, game_count), sizeof(entry_t));
        if (!order || !entries) {
            games_free();
            return;
        }
        for (int i = 0; i < game_count; i++) {
            order[i] = i;
            entries[i] = (entry_t) { .name = games[i].path, .type = ENTRY_TYPE_ROM };
        }
        sort_games(false);
        return;
    }

    path_t *dir = scan.stack[--scan.depth];
    int level = scan.levels[scan.depth];
    dir_t info;
    int result = dir_findfirst(path_get(dir), &info);
    while (result == 0) {
        path_push(dir, info.d_name);
        // The menu's own folder (metadata, screenshots, box art: thousands of folders, no games)
        // and system folders, even with Show Hidden Files on.
        if (!view_browser_is_protected(dir)) {
            if (info.d_type == DT_DIR) {
                if (level < SCAN_MAX_DEPTH && strcmp(info.d_name, SAVE_DIRECTORY_NAME) != 0) {
                    scan_push(path_clone(dir), level + 1);
                }
            } else if (view_browser_is_rom_file(info.d_name)) {
                if (!hidden_contains(dir) || menu->settings.show_hidden_games) {
                    add_game(dir);
                }
            }
        }
        path_pop(dir);
        result = dir_findnext(path_get(dir), &info);
    }
    path_free(dir);
    scan.folders++;
    sound_poll();
}

/** @brief Read titles in the background, and swap in metadata titles as they're cached. */
static void titles_update (void) {
    if (titles.done || game_count == 0) {
        return;
    }
    if (!titles.fetched) {
        titles.fetched = ui_components_game_info_prefetch(NULL, entries, game_count, 0, GAME_INFO_PREFETCH_IDLE_US);
    }
    int start = titles.cursor;
    for (int n = 0; n < TITLE_CHECKS; n++) {
        game_t *game = &games[titles.cursor];
        if (!game->title_final && ui_components_game_info_is_cached(game->path)) {
            const char *title = ui_components_game_info_cached_title(game->path);
            if (title) {
                char *copy = strdup(title);
                if (copy) {
                    free(game->title);
                    game->title = copy;
                }
            }
            game->title_final = true;
        }
        titles.cursor = (titles.cursor + 1) % game_count;
        if (titles.cursor == 0 && titles.fetched) {
            // A full pass since the cache finished: the titles are as good as they get.
            titles.done = true;
            if (sort_column == COLUMN_TITLE) {
                sort_games(true);
            }
            return;
        }
        if (titles.cursor == start) {
            break;
        }
    }
}


static void play_selected (menu_t *menu) {
    game_t *game = selected_game();
    if (!game) {
        return;
    }
    if (menu->load.request_path) {
        path_free(menu->load.request_path);
    }
    menu->load.request_path = path_create(game->path);
    menu->load.load_history_id = -1;
    menu->load.load_favorite_id = -1;
    menu->load.return_mode = MENU_MODE_HISTORY;
    menu->load.play_now = true;
    menu->load.open_configure = false;
    menu->next_mode = MENU_MODE_LOAD_ROM;
    loading_started_ms = get_ticks_ms();
}

static void process (menu_t *menu) {
    if (ui_components_tab_process(menu, TAB_HISTORY)) {
        return;
    }
    if (!scan.done || game_count == 0) {
        return;
    }

    int rows = rows_on_page();
    if (menu->actions.enter || menu->actions.settings) {     // A or Start plays
        sound_play_effect(SFX_ENTER);
        play_selected(menu);
    } else if (menu->actions.c_left || menu->actions.c_right) {
        int next = page + (menu->actions.c_right ? 1 : -1);
        if (next >= 0 && next < page_count()) {
            page = next;
            selected = MIN(selected, rows_on_page() - 1);
            first_visible = MIN(first_visible, MAX(0, rows_on_page() - VISIBLE_ROWS));
            sound_play_effect(SFX_CURSOR);
        } else {
            sound_play_effect(SFX_ERROR);
        }
    } else if (menu->actions.c_up) {
        descending = !descending;
        sort_games(false);
        sound_play_effect(SFX_SETTING);
    } else if ((menu->actions.go_left || menu->actions.go_right) && !menu->actions.go_fast) {
        sort_column = (sort_column + (menu->actions.go_right ? 1 : COLUMN_COUNT - 1)) % COLUMN_COUNT;
        descending = (sort_column != COLUMN_TITLE);     // titles A-Z; counts and dates highest first
        sort_games(false);
        sound_play_effect(SFX_SETTING);
    } else if ((menu->actions.go_up || menu->actions.go_down) && !menu->actions.go_fast) {
        int next = selected + (menu->actions.go_down ? 1 : -1);
        if (next >= 0 && next < rows) {
            selected = next;
            sound_play_effect(SFX_CURSOR);
        }
    }
}


/** @brief Small triangle after the sorted column's name: pointing up (A-Z, lowest first) or down. */
static void draw_sort_arrow (int x, int baseline, bool down) {
    int cap = fonts_cap_height(BODY_FONT);
    int top = baseline - cap + ((cap - 4) / 2);
    for (int row = 0; row < 4; row++) {
        int half = down ? (3 - row) : row;
        ui_components_box_draw(x + 3 - half, top + row, x + 4 + half, top + row + 1, TEXT_COLOR);
    }
}

static int body_text_width (const char *text) {
    int nbytes = strlen(text);
    rdpq_paragraph_t *layout = rdpq_paragraph_build(&(rdpq_textparms_t) { 0 }, BODY_FONT, text, &nbytes);
    int width = (int) layout->advance_x;
    rdpq_paragraph_free(layout);
    return width;
}

static int header_y (void) {
    return CHEAT_LIST_Y - fonts_cap_height(TITLE_FONT) - 12;
}

static void draw_column_headers (void) {
    int y = header_y();
    const int xs[COLUMN_COUNT] = { TITLE_X, PLAY_COUNT_X, LAST_PLAYED_X };
    ui_components_body_text_draw(&(rdpq_textparms_t) { .style_id = STL_GRAY }, NUMBER_X, y, "#");
    for (int c = 0; c < COLUMN_COUNT; c++) {
        bool sorted = (c == (int) sort_column);
        ui_components_body_text_draw(&(rdpq_textparms_t) { .style_id = sorted ? STL_DEFAULT : STL_GRAY }, xs[c], y, column_names[c]);
        if (sorted) {
            draw_sort_arrow(xs[c] + body_text_width(column_names[c]) + 4, y, descending);
        }
    }
}

static const char *format_date (time_t t, char *buffer, size_t size) {
    struct tm *tm = (t > 0) ? gmtime(&t) : NULL;
    if (!tm || tm->tm_year + 1900 < 1990) {
        return "-";
    }
    strftime(buffer, size, "%b %d, %Y", tm);
    for (char *c = buffer; *c; c++) {
        *c = toupper((unsigned char) (*c));
    }
    return buffer;
}

static void draw_table (void) {
    draw_column_headers();

    int rows = rows_on_page();
    if (selected < first_visible) {
        first_visible = selected;
    } else if (selected >= first_visible + VISIBLE_ROWS) {
        first_visible = selected - VISIBLE_ROWS + 1;
    }

    for (int row = 0; row < VISIBLE_ROWS && first_visible + row < rows; row++) {
        int i = first_visible + row;
        int index = (page * PAGE_ROWS) + i;
        game_t *game = &games[order[index]];
        int y = CHEAT_LIST_Y + (row * CHEAT_ROW_PITCH);
        bool is_selected = (i == selected);
        menu_font_style_t style = is_selected ? STL_DEFAULT : STL_GRAY;

        if (is_selected) {
            int cap = fonts_cap_height(TITLE_FONT);
            ui_components_box_draw(CAROUSEL_SELECTED_X, y - cap - 3, CAROUSEL_SELECTED_X + 4, y + 3, SELECTION_MARKER_COLOR);
        }
        ui_components_body_text_printf(&(rdpq_textparms_t) { .style_id = STL_GRAY }, NUMBER_X, y, "%d", index + 1);
        ui_components_text_draw(
            &(rdpq_textparms_t) { .style_id = style, .width = PLAY_COUNT_X - TITLE_X - COLUMN_GAP, .wrap = WRAP_ELLIPSES },
            TITLE_FONT, TITLE_X, y, game->title
        );
        rdpq_text_printf(&(rdpq_textparms_t) { .style_id = style }, TITLE_FONT, PLAY_COUNT_X, y, "%lu", (unsigned long) game->plays);
        char date[32];
        rdpq_text_printf(&(rdpq_textparms_t) { .style_id = style }, TITLE_FONT, LAST_PLAYED_X, y, "%s",
            format_date(game->last_played, date, sizeof(date)));
    }

    ui_components_table_scrollbar_draw(CHEAT_LIST_Y, CHEAT_ROW_PITCH, first_visible, rows, VISIBLE_ROWS);
}

static void draw_message (const char *text) {
    ui_components_body_text_draw(
        &(rdpq_textparms_t) { .style_id = STL_GRAY, .width = VISIBLE_AREA_X1 - NUMBER_X, .wrap = WRAP_WORD },
        NUMBER_X, CHEAT_LIST_Y, text
    );
}

/** @brief Tabs and table; show_hints is false while a game loads (the table then fades out). */
static void draw_content (menu_t *menu, bool show_hints) {
    ui_components_tab_header_draw(TAB_HISTORY);

    if (!scan.done) {
        char text[192];
        snprintf(text, sizeof(text), "Finding games in %s... %d found (%d folders searched)", scan.folder, game_count, scan.folders);
        draw_message(text);
        return;
    }
    if (game_count == 0) {
        char text[192];
        snprintf(text, sizeof(text), "No games found in %s.", scan.folder ? scan.folder : "/");
        draw_message(text);
        return;
    }

    draw_table();

    char help[96];
    snprintf(help, sizeof(help), "Page %d of %d. Left / Right: sort by another column.", page + 1, page_count());
    ui_components_body_text_draw(
        &(rdpq_textparms_t) { .style_id = STL_GRAY, .width = VISIBLE_AREA_X1 - NUMBER_X },
        NUMBER_X, CHEAT_LIST_Y + (VISIBLE_ROWS * CHEAT_ROW_PITCH), help
    );

    if (!show_hints) {
        // Loading a game: fade the table out (the eclipse plays in its place).
        uint32_t since = get_ticks_ms() - loading_started_ms;
        uint8_t alpha = (since >= TABLE_FADE_MS) ? 0xFF : (uint8_t) ((since * 0xFF) / TABLE_FADE_MS);
        rdpq_mode_push();
            rdpq_set_mode_standard();
            rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
            rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
            rdpq_set_prim_color(PALETTE_WITH_ALPHA(PALETTE_BACKGROUND, alpha));
            rdpq_fill_rectangle(0, header_y() - fonts_cap_height(BODY_FONT) - 4, DISPLAY_WIDTH, LIBRARY_BUTTONS_Y - 24);
        rdpq_mode_pop();
        return;
    }

    button_hint_t hints[BUTTON_HINTS_MAX];
    int count = 0;
    hints[count++] = (button_hint_t) { ICON_A, "Play", ICON_START, true };     // A or Start
    if (page_count() > 1) {
        hints[count++] = (button_hint_t) { ICON_C_LEFT, "Page", ICON_C_RIGHT, true };
    }
    hints[count++] = (button_hint_t) { ICON_C_UP, "Reverse" };
    ui_components_button_hints_draw(hints, count);
}

static void draw (menu_t *menu, surface_t *display) {
    ui_components_attach_clear(display);
    draw_content(menu, true);
    rdpq_detach_show();
}


void view_history_draw_behind_loading (menu_t *menu) {
    draw_content(menu, false);
}

int view_history_loading_center_y (void) {
    int top = header_y() - fonts_cap_height(BODY_FONT);
    int bottom = CHEAT_LIST_Y + (VISIBLE_ROWS * CHEAT_ROW_PITCH);
    return (top + bottom) / 2;
}

void view_history_init (menu_t *menu) {
    bool settings_changed = scan.started &&
        (scan.show_hidden != menu->settings.show_hidden_games ||
         strcmp(scan.folder ? scan.folder : "", start_folder(menu)) != 0);
    if (!scan.started || settings_changed) {
        scan_start(menu);
    }
}

void view_history_display (menu_t *menu, surface_t *display) {
    if (!scan.done) {
        uint64_t start = get_ticks_us();
        do {
            scan_step(menu);
        } while (!scan.done && (get_ticks_us() - start) < SCAN_BUDGET_US);
    } else {
        titles_update();
    }
    process(menu);
    draw(menu, display);
}
