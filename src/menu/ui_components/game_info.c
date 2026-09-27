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
#include "../sound.h"
#include "constants.h"
#include "utils/utils.h"

#define MAX_PLAYERS         (4)
#define BADGE_PADDING       (3)
#define BADGE_GAP           (4)
#define GAME_INFO_PAGES     (3)

/** @brief Info for the currently selected entry, reloaded only when the selection changes. */
static struct {
    char *path;
    bool is_rom;
    rom_info_t rom_info;
    time_t added;
    int64_t size;
    time_t last_played;
} current;

// About page scrolling, in lines of text.
#define PAGE_ABOUT              (2)
static int last_page_drawn = -1;
static int value_right = VISIBLE_AREA_X1;   // values stop here (short of the screenshot on the Overview page)
static int about_scroll = 0;
static int about_max_scroll = 0;


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
    about_scroll = 0;

    struct stat st;
    if (stat(path_get(path), &st) == 0) {
        current.added = st.st_mtime;
        current.size = st.st_size;
    }
    current.last_played = bookkeeping_history_last_played(bookkeeping, path);
    sound_poll();

    if (entry->type == ENTRY_TYPE_ROM) {
        current.is_rom = (rom_config_load(path, &current.rom_info) == ROM_OK);
        sound_poll();
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

static void draw_shadowed (rdpq_textparms_t parms, int x, int y, const char *text) {
    ui_components_body_text_draw(&parms, x, y, text);
}

/** @brief Lay out a paragraph twice (shadow and text) and draw both. */
static void render_paragraph_with_shadow (rdpq_textparms_t parms, int x, int y, const char *text, menu_font_style_t shadow_style) {
    menu_font_style_t style = parms.style_id;
    for (int layer = 0; layer < 2; layer++) {
        int nbytes = strlen(text);
        parms.style_id = (layer == 0) ? shadow_style : style;
        rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, GAME_INFO_FONT, text, &nbytes);
        int offset = (layer == 0) ? TEXT_SHADOW_OFFSET : 0;
        rdpq_paragraph_render(layout, x + offset, y + offset);
        rdpq_paragraph_free(layout);
    }
}

static void draw_text (int x, int y, menu_font_style_t style, const char *text) {
    char upper[128];
    snprintf(upper, sizeof(upper), "%s", text);
    for (char *c = upper; *c; c++) {
        *c = toupper((unsigned char) (*c));
    }
    int right = (x >= GAME_INFO_VALUE_X) ? value_right : VISIBLE_AREA_X1;
    draw_shadowed((rdpq_textparms_t) { .style_id = style, .width = right - x, .wrap = WRAP_ELLIPSES }, x, y, upper);
}

/** @brief Grey badge with white text for the RUMBLE PAK / USA style tags. Returns its width. */
static int draw_badge (int x, int y, const char *text) {
    int nbytes = strlen(text);
    rdpq_paragraph_t *layout = rdpq_paragraph_build(&(rdpq_textparms_t) { 0 }, GAME_INFO_FONT, text, &nbytes);
    int width = (int) (layout->advance_x) + (BADGE_PADDING * 2);
    rdpq_paragraph_free(layout);
    if (x + width > value_right) {
        return -1;
    }
    ui_components_box_draw(x, GAME_INFO_BADGE_TOP(y), x + width, y + 3, GAME_INFO_BADGE_COLOR);
    // The badge is the same tone as the normal shadow, so its text gets the dark one.
    render_paragraph_with_shadow((rdpq_textparms_t) { .style_id = STL_DEFAULT }, x + BADGE_PADDING, y, text, STL_SHADOW_DARK);
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
    ui_components_box_draw(x, GAME_INFO_BADGE_TOP(y), x + width, y + 3, GAME_INFO_BADGE_COLOR);
    for (uint32_t i = 0; i < MAX_PLAYERS; i++) {
        draw_player_icon(x + BADGE_PADDING + (i * 7), y - 8, (i < players) ? GAME_INFO_PLAYER_ON_COLOR : GAME_INFO_PLAYER_OFF_COLOR);
    }
}

static void draw_accessories (int x, int y, rom_info_t *info) {
    const char *badges[6];
    int count = 0;

    if (info->features.rumble_pak) badges[count++] = "RMB PAK";
    if (info->features.controller_pak) badges[count++] = "CTL PAK";
    if (info->features.transfer_pak) badges[count++] = "TRN PAK";
    if (info->features.expansion_pak == EXPANSION_PAK_REQUIRED) badges[count++] = "EXP PAK";
    else if (info->features.expansion_pak == EXPANSION_PAK_RECOMMENDED) badges[count++] = "EXP PAK+";
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
    draw_text(GAME_INFO_VALUE_X, y, STL_DEFAULT, value ? value : "-");
}

static const char *format_save_type (rom_save_type_t save_type) {
    switch (save_type) {
        case SAVE_TYPE_NONE: return "None";
        case SAVE_TYPE_EEPROM_4KBIT: return "EEPROM 4Kbit";
        case SAVE_TYPE_EEPROM_16KBIT: return "EEPROM 16Kbit";
        case SAVE_TYPE_SRAM_256KBIT: return "SRAM 256Kbit";
        case SAVE_TYPE_SRAM_BANKED: return "SRAM 768Kbit (3 banks)";
        case SAVE_TYPE_SRAM_1MBIT: return "SRAM 1Mbit";
        case SAVE_TYPE_FLASHRAM_1MBIT: return "FlashRAM 1Mbit";
        case SAVE_TYPE_FLASHRAM_PKST2: return "FlashRAM (Pokemon Stadium 2)";
        default: return NULL;
    }
}

static const char *format_cic (rom_cic_type_t cic_type) {
    switch (cic_type) {
        case ROM_CIC_TYPE_5101: return "5101";
        case ROM_CIC_TYPE_5167: return "5167";
        case ROM_CIC_TYPE_6101: return "6101";
        case ROM_CIC_TYPE_7102: return "7102";
        case ROM_CIC_TYPE_x102: return "6102 / 7101";
        case ROM_CIC_TYPE_x103: return "6103 / 7103";
        case ROM_CIC_TYPE_x105: return "6105 / 7105";
        case ROM_CIC_TYPE_x106: return "6106 / 7106";
        case ROM_CIC_TYPE_8301: return "8301";
        case ROM_CIC_TYPE_8302: return "8302";
        case ROM_CIC_TYPE_8303: return "8303";
        case ROM_CIC_TYPE_8401: return "8401";
        case ROM_CIC_TYPE_8501: return "8501";
        default: return NULL;
    }
}

static const char *format_video (rom_tv_type_t tv_type) {
    switch (tv_type) {
        case ROM_TV_TYPE_PAL: return "PAL";
        case ROM_TV_TYPE_NTSC: return "NTSC";
        case ROM_TV_TYPE_MPAL: return "MPAL";
        default: return NULL;
    }
}

static const char *format_size (int64_t bytes, char *buffer, size_t size) {
    if (bytes >= (1024 * 1024)) {
        snprintf(buffer, size, "%lld MB", (long long) (bytes / (1024 * 1024)));
    } else {
        snprintf(buffer, size, "%lld KB", (long long) (bytes / 1024));
    }
    return buffer;
}

/** @brief Page 1: players, accessories, region, credits and dates (the mockup layout). */
static void draw_overview_page (entry_t *entry, rom_info_t *info) {
    int x = GAME_INFO_VALUE_X;
    int y = GAME_INFO_Y;
    char date[32];

    // Right half: the game's screenshot (a placeholder for now); values stop short of it.
    if (entry && entry->type == ENTRY_TYPE_ROM) {
        value_right = GAME_INFO_SCREENSHOT_X - GAME_INFO_SCREENSHOT_GAP;
        ui_components_box_draw(GAME_INFO_SCREENSHOT_X, GAME_INFO_SCREENSHOT_Y,
            GAME_INFO_SCREENSHOT_X + GAME_INFO_SCREENSHOT_WIDTH, GAME_INFO_SCREENSHOT_Y + GAME_INFO_SCREENSHOT_HEIGHT,
            GAME_INFO_SCREENSHOT_PLACEHOLDER_COLOR);
    }

    draw_row(y, "Player Count");
    draw_player_count(x, y, info ? info->meta.num_players : 0);
    y += GAME_INFO_ROW_PITCH;

    draw_row(y, "Accessories");
    if (info) {
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
    draw_value(y, info ? meta_value(info->meta.publisher) : NULL);
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

/** @brief Page 2: header and boot details. */
static void draw_details_page (entry_t *entry, rom_info_t *info) {
    int y = GAME_INFO_Y;
    char buffer[32];

    draw_row(y, "Game Code");
    snprintf(buffer, sizeof(buffer), "%.4s", info->game_code);
    draw_value(y, buffer);
    y += GAME_INFO_ROW_PITCH;

    draw_row(y, "Version");
    snprintf(buffer, sizeof(buffer), "1.%d", info->version);
    draw_value(y, buffer);
    y += GAME_INFO_ROW_PITCH;

    draw_row(y, "Size");
    // Directory listings don't always carry sizes (e.g. the emulator's rom:/ filesystem), so use stat().
    draw_value(y, format_size(current.size ? current.size : entry->size, buffer, sizeof(buffer)));
    y += GAME_INFO_ROW_PITCH + GAME_INFO_GROUP_GAP;

    draw_row(y, "Save Type");
    draw_value(y, format_save_type(rom_info_get_save_type(info)));
    y += GAME_INFO_ROW_PITCH;

    draw_row(y, "CIC");
    draw_value(y, format_cic(rom_info_get_cic_type(info)));
    y += GAME_INFO_ROW_PITCH;

    draw_row(y, "Video");
    draw_value(y, format_video(rom_info_get_tv_type(info)));
    y += GAME_INFO_ROW_PITCH + GAME_INFO_GROUP_GAP;

    draw_row(y, "Cheats");
    draw_value(y, info->settings.cheats_enabled ? "On" : "Off");
    y += GAME_INFO_ROW_PITCH;

    draw_row(y, "Clear RDRAM");
    draw_value(y, info->settings.clear_rdram_enabled ? "On" : "Off");
}

/** @brief Page 3: description and links from the game's metadata. */
/** @brief Page 3: the game's description, using the whole info area; scrolls when it doesn't fit. */
static void draw_about_page (rom_info_t *info) {
    const char *description = meta_value(info->meta.short_description);
    int x = GAME_INFO_LABEL_X;
    int top = GAME_INFO_Y - GAME_INFO_ABOUT_ASCENT;
    int bottom = GAME_INFO_ABOUT_BOTTOM;

    int nbytes = strlen(description ? description : "");
    if (!description) {
        about_max_scroll = 0;
        draw_shadowed(
            (rdpq_textparms_t) { .style_id = STL_GRAY, .width = VISIBLE_AREA_X1 - x, .wrap = WRAP_WORD },
            x, GAME_INFO_Y, "No description yet. Add a metadata file for this game to show one here."
        );
        return;
    }

    rdpq_textparms_t parms = { .style_id = STL_DEFAULT, .width = VISIBLE_AREA_X1 - x, .wrap = WRAP_WORD };
    rdpq_paragraph_t *layout = rdpq_paragraph_build(&parms, GAME_INFO_FONT, description, &nbytes);

    int line_height = GAME_INFO_ABOUT_LINE_HEIGHT;
    int visible_lines = (bottom - top) / line_height;
    about_max_scroll = MAX(0, layout->nlines - visible_lines);
    if (about_scroll > about_max_scroll) {
        about_scroll = about_max_scroll;
    }

    // Clip to the info area and shift the text up by the scrolled lines.
    rdpq_set_scissor(0, top, DISPLAY_WIDTH, bottom);
    rdpq_paragraph_free(layout);
    render_paragraph_with_shadow(parms, x, GAME_INFO_Y - (about_scroll * line_height), description, STL_SHADOW);
    rdpq_set_scissor(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    // "..." where there is more text above or below.
    rdpq_textparms_t hint = { .style_id = STL_GRAY };
    if (about_scroll > 0) {
        draw_shadowed(hint, VISIBLE_AREA_X1 - 12, top - 4, "...");
    }
    if (about_scroll < about_max_scroll) {
        draw_shadowed(hint, VISIBLE_AREA_X1 - 12, bottom + 10, "...");
    }
}

/**
 * @brief Scroll the About page's description by one line.
 *
 * @param direction -1 (up) or +1 (down).
 * @return true if it scrolled; false if the About page isn't showing or the text is already at
 *         that end (so up / down should change the page instead).
 */
bool ui_components_game_info_scroll (int direction) {
    if (last_page_drawn != PAGE_ABOUT) {
        return false;
    }
    int next = about_scroll + direction;
    if (next < 0 || next > about_max_scroll) {
        return false;
    }
    about_scroll = next;
    return true;
}

/**
 * @brief Forget the cached info (e.g. after a game's settings changed).
 */
void ui_components_game_info_invalidate (void) {
    current_free();
}

/**
 * @brief Number of info pages available for an entry (0 for folders).
 */
int ui_components_game_info_page_count (entry_t *entry) {
    if (!entry || entry->type == ENTRY_TYPE_DIR) {
        return 0;
    }
    return (entry->type == ENTRY_TYPE_ROM) ? GAME_INFO_PAGES : 1;
}

/**
 * @brief Draw one page of the info panel for the selected Library entry.
 */
void ui_components_game_info_draw (path_t *directory, entry_t *entry, bookkeeping_t *bookkeeping, int page) {
    if (ui_components_game_info_page_count(entry) == 0) {
        return;
    }

    path_t *path = directory ? path_clone_push(directory, entry->name) : path_create(entry->name);
    current_load(path, entry, bookkeeping);
    path_free(path);

    rom_info_t *info = current.is_rom ? &current.rom_info : NULL;

    if (page != last_page_drawn) {
        about_scroll = 0;
    }
    last_page_drawn = page;
    value_right = VISIBLE_AREA_X1;     // the Overview page narrows it for the screenshot

    if (page == 1 && info) {
        draw_details_page(entry, info);
    } else if (page == PAGE_ABOUT && info) {
        draw_about_page(info);
    } else {
        draw_overview_page(entry, info);
    }
}

/**
 * @brief Draw the page indicator dots in a column just left of the title, centred on its capitals.
 */
void ui_components_game_info_dots_draw (int page, int count) {
    if (count < 2) {
        return;
    }
    int column_height = GAME_INFO_DOT_SIZE + ((count - 1) * GAME_INFO_DOT_PITCH);
    int top = CAROUSEL_TITLE_Y - (fonts_cap_height(FNT_TITLE) / 2) - (column_height / 2);
    for (int i = 0; i < count; i++) {
        int x = GAME_INFO_LABEL_X;
        int y = top + (i * GAME_INFO_DOT_PITCH);
        color_t color = (i == page) ? GAME_INFO_DOT_ON_COLOR : GAME_INFO_DOT_OFF_COLOR;
        // 6x6 dot with the corners cut off
        ui_components_box_draw(x + 1, y, x + 5, y + 6, color);
        ui_components_box_draw(x, y + 1, x + 6, y + 5, color);
    }
}
