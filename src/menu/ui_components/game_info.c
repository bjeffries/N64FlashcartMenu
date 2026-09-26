/**
 * @file game_info.c
 * @brief Library info panel: player count, accessories, region, credits and dates of the selected game
 * @ingroup ui_components
 */

#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "../ui_components.h"
#include "../fonts.h"
#include "constants.h"

#define MAX_PLAYERS         (4)
#define BADGE_PADDING       (3)
#define BADGE_GAP           (4)

/** @brief Info for the currently selected entry, reloaded only when the selection changes. */
static struct {
    char *path;
    bool is_rom;
    rom_info_t rom_info;
    time_t added;
    time_t last_played;
} current;


static void current_free (void) {
    if (current.is_rom) {
        rom_info_free_meta(&current.rom_info);
    }
    free(current.path);
    memset(&current, 0, sizeof(current));
}

static void current_load (path_t *path, entry_t *entry, bookkeeping_t *bookkeeping) {
    if (current.path && strcmp(current.path, path_get(path)) == 0) {
        return;
    }

    current_free();
    current.path = strdup(path_get(path));

    struct stat st;
    if (stat(path_get(path), &st) == 0) {
        current.added = st.st_mtime;
    }
    current.last_played = bookkeeping_history_last_played(bookkeeping, path);

    if (entry->type == ENTRY_TYPE_ROM) {
        current.is_rom = (rom_config_load(path, &current.rom_info) == ROM_OK);
    }
}

static const char *format_region (rom_destination_type_t code) {
    switch (code) {
        case MARKET_NORTH_AMERICA: return "USA";
        case MARKET_JAPANESE:
        case MARKET_JAPANESE_MULTI: return "JAPAN";
        case MARKET_EUROPEAN_BASIC:
        case MARKET_OTHER_X:
        case MARKET_OTHER_Y: return "EUROPE";
        case MARKET_GERMAN: return "GERMANY";
        case MARKET_FRENCH: return "FRANCE";
        case MARKET_ITALIAN: return "ITALY";
        case MARKET_SPANISH: return "SPAIN";
        case MARKET_DUTCH: return "NETHERLANDS";
        case MARKET_SCANDINAVIAN: return "SCANDINAVIA";
        case MARKET_AUSTRALIAN: return "AUSTRALIA";
        case MARKET_CANADIAN: return "CANADA";
        case MARKET_BRAZILIAN: return "BRAZIL";
        case MARKET_CHINESE: return "CHINA";
        case MARKET_KOREAN: return "KOREA";
        case MARKET_GATEWAY64_NTSC:
        case MARKET_GATEWAY64_PAL: return "LODGENET";
        default: return NULL;
    }
}

/** @brief Metadata strings default to "Not specified"; treat that and "" as missing. */
static const char *meta_value (const char *value) {
    if (!value || value[0] == '\0' || strcmp(value, "Not specified") == 0) {
        return NULL;
    }
    return value;
}

static const char *format_date (time_t t, char *buffer, size_t size) {
    struct tm *tm = (t > 0) ? gmtime(&t) : NULL;
    // FAT timestamps start in 1980; anything that early means "no real date".
    if (!tm || tm->tm_year + 1900 < 1990) {
        return NULL;
    }
    strftime(buffer, size, "%b %d, %Y", tm);
    return buffer;
}

static void draw_text (int x, int y, menu_font_style_t style, const char *text) {
    char upper[128];
    snprintf(upper, sizeof(upper), "%s", text);
    for (char *c = upper; *c; c++) {
        *c = toupper((unsigned char) (*c));
    }
    ui_components_text_draw(
        &(rdpq_textparms_t) { .style_id = style, .width = VISIBLE_AREA_X1 - x, .wrap = WRAP_ELLIPSES },
        FNT_SMALL, x, y, upper
    );
}

/** @brief White badge with black text, like the mockup's RUMBLE PAK / USA tags. Returns its width. */
static int draw_badge (int x, int y, const char *text) {
    int nbytes = strlen(text);
    rdpq_paragraph_t *layout = rdpq_paragraph_build(
        &(rdpq_textparms_t) { .style_id = STL_BLACK },
        FNT_SMALL, text, &nbytes
    );
    int width = (int) (layout->advance_x) + (BADGE_PADDING * 2);
    if (x + width > VISIBLE_AREA_X1) {
        rdpq_paragraph_free(layout);
        return -1;
    }
    ui_components_box_draw(x, y - 10, x + width, y + 3, GAME_INFO_BADGE_COLOR);
    rdpq_paragraph_render(layout, x + BADGE_PADDING, y);
    rdpq_paragraph_free(layout);
    return width;
}

static void draw_player_icon (int x, int y, color_t color) {
    ui_components_box_draw(x + 1, y, x + 4, y + 3, color);      // head
    ui_components_box_draw(x, y + 4, x + 5, y + 7, color);      // shoulders
}

static void draw_player_count (int x, int y, uint32_t players) {
    if (players == 0 || players > MAX_PLAYERS) {
        draw_text(x, y, STL_DEFAULT, "-");
        return;
    }
    int width = (BADGE_PADDING * 2) + (MAX_PLAYERS * 7) - 2;
    ui_components_box_draw(x, y - 10, x + width, y + 3, GAME_INFO_BADGE_COLOR);
    for (uint32_t i = 0; i < MAX_PLAYERS; i++) {
        draw_player_icon(x + BADGE_PADDING + (i * 7), y - 8, (i < players) ? RGBA32(0, 0, 0, 0xFF) : GAME_INFO_PLAYER_OFF_COLOR);
    }
}

static void draw_accessories (int x, int y, rom_info_t *info) {
    const char *badges[6];
    int count = 0;

    if (info->features.rumble_pak) badges[count++] = "RUMBLE PAK";
    if (info->features.controller_pak) badges[count++] = "CONTROLLER PAK";
    if (info->features.transfer_pak) badges[count++] = "TRANSFER PAK";
    if (info->features.expansion_pak == EXPANSION_PAK_REQUIRED) badges[count++] = "EXPANSION PAK";
    else if (info->features.expansion_pak == EXPANSION_PAK_RECOMMENDED) badges[count++] = "EXPANSION PAK+";
    if (info->features.voice_recognition_unit) badges[count++] = "VRU";

    if (count == 0) {
        draw_text(x, y, STL_DEFAULT, "-");
        return;
    }
    for (int i = 0; i < count; i++) {
        int width = draw_badge(x, y, badges[i]);
        if (width < 0) {
            break;
        }
        x += width + BADGE_GAP;
    }
}

static void draw_row (int y, const char *label) {
    draw_text(GAME_INFO_LABEL_X, y, STL_DEFAULT, label);
}

static void draw_value (int y, const char *value) {
    draw_text(CAROUSEL_SELECTED_X, y, STL_DEFAULT, value ? value : "-");
}

/**
 * @brief Forget the cached info (e.g. after a game's settings changed).
 */
void ui_components_game_info_invalidate (void) {
    current_free();
}

/**
 * @brief Draw the info panel for the selected Library entry.
 */
void ui_components_game_info_draw (path_t *directory, entry_t *entry, bookkeeping_t *bookkeeping) {
    if (!entry || entry->type == ENTRY_TYPE_DIR) {
        return;
    }

    path_t *path = path_clone_push(directory, entry->name);
    current_load(path, entry, bookkeeping);
    path_free(path);

    rom_info_t *info = current.is_rom ? &current.rom_info : NULL;
    int x = CAROUSEL_SELECTED_X;
    int y = GAME_INFO_Y;
    char date[32];

    draw_row(y, "Player Count");
    draw_player_count(x, y, info ? info->meta.num_players : 0);
    y += GAME_INFO_ROW_PITCH;

    draw_row(y, "Accessories");
    if (entry->type == ENTRY_TYPE_DISK) {
        draw_badge(x, y, "64DD");
    } else if (info) {
        draw_accessories(x, y, info);
    } else {
        draw_value(y, NULL);
    }
    y += GAME_INFO_ROW_PITCH;

    draw_row(y, "Region");
    const char *region = info ? format_region(info->destination_code) : NULL;
    if (region) {
        draw_badge(x, y, region);
    } else {
        draw_value(y, NULL);
    }
    y += GAME_INFO_ROW_PITCH + GAME_INFO_GROUP_GAP;

    draw_row(y, "Developers");
    draw_value(y, info ? meta_value(info->meta.author) : NULL);
    y += GAME_INFO_ROW_PITCH;

    draw_row(y, "Publishers");
    draw_value(y, NULL);
    y += GAME_INFO_ROW_PITCH;

    draw_row(y, "Release");
    draw_value(y, info ? meta_value(info->meta.release_date) : NULL);
    y += GAME_INFO_ROW_PITCH + GAME_INFO_GROUP_GAP;

    draw_row(y, "Added");
    draw_value(y, format_date(current.added, date, sizeof(date)));
    y += GAME_INFO_ROW_PITCH;

    draw_row(y, "Last Played");
    draw_value(y, format_date(current.last_played, date, sizeof(date)));
}
