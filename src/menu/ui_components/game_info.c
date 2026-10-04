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
#include "../png_decoder.h"
#include "../n64_logo_frames.h"
#include "../sound.h"
#include "constants.h"
#include "utils/fs.h"
#include "utils/utils.h"

#define MAX_PLAYERS         (4)
#define BADGE_PADDING       (3)
#define TAG_GAP             (12)    // between accessory tags, with a small square in the middle
#define TAG_SEPARATOR_SIZE  (3)
#define GAME_INFO_PAGES     (3)

/*
 * Info cache: what the Overview and Details pages show for each game, read from the SD card once
 * (several reads per game: the file's date and size, the ROM header and config, its metadata and a
 * screenshot check) and kept in memory. It's filled a few games per frame in the background
 * (ui_components_game_info_prefetch) during the boot animation and whenever the carousel is
 * still, nearest the selection first, so scrolling shows cached games' info straight away; a
 * game that isn't cached yet is read once the carousel settles. The About page's description is
 * cached too with an Expansion Pak; without one (4MB) it's read when that page is shown.
 */
#define INFO_TEXT               (48)
typedef struct {
    char *path;                 // NULL: free slot
    uint32_t hash;
    int next;                   // next in its bucket, or -1
    bool is_rom;
    bool has_screenshot;        // screenshot_1.png exists
    time_t added;
    int64_t size;
    char game_code[4];
    uint8_t version;
    uint8_t players;
    rom_destination_type_t region;
    bool rumble_pak, controller_pak, transfer_pak, vru;
    rom_expansion_pak_t expansion_pak;
    rom_save_type_t save_type;
    rom_cic_type_t cic;
    rom_tv_type_t tv;
    bool cheats, clear_rdram;
    char developer[INFO_TEXT];  // "" when unknown
    char publisher[INFO_TEXT];
    char release[INFO_TEXT / 2];
    bool has_description;       // description below is cached (Expansion Pak only)
    char *description;          // NULL when there's none
} game_summary_t;

#define INFO_CACHE_BUCKETS          (1024)
#define INFO_CACHE_SIZE             (2048)  // games kept with an Expansion Pak...
#define INFO_CACHE_SIZE_SMALL       (512)   // ...and without one (4MB)
static struct {
    game_summary_t *entries;
    int capacity;
    int used;
    int evict;                  // next slot to reuse once full (oldest first)
    int buckets[INFO_CACHE_BUCKETS];
} cache;

/** @brief Background filling: the list being worked through, outward from where it started. */
static struct {
    const entry_t *list;
    int32_t count;
    char *directory;
    uint32_t signature;         // first and last names: Faves and History share one array
    int32_t origin;
    int32_t done;               // entries visited, in the order origin, +1, -1, +2, -2, ...
} prefetch;

/** @brief The selected entry: the full info (About page) and screenshots follow it. */
static struct {
    char *path;
    bool shots_selected;
    bool about_loaded;
    bool about_is_rom;
    rom_info_t about;
} current;

// About page scrolling, in lines of text.
#define PAGE_ABOUT              (2)
static int last_page_drawn = -1;
static int value_right = VISIBLE_AREA_X1;   // values stop here (short of the screenshot on the Overview page)
static int about_scroll = 0;
static int about_max_scroll = 0;

/*
 * Screenshots on the Overview page: menu/metadata/A/B/C/D/screenshot_N.png (make_screenshots.py),
 * decoded in the background by the PNG decoder. The first is decoded once the selection has
 * settled; with several, the next is decoded every SCREENSHOT_CYCLE_MS and swapped in.
 */
#define SCREENSHOT_MAX          (9)
#define SCREENSHOT_SETTLE_MS    (250)   // carousel still this long: don't count or decode while scrolling past games
#define SCREENSHOT_CYCLE_MS     (2500)
static struct {
    char base[256];             // ".../screenshot_" (the number and ".png" follow)
    bool has_any;               // screenshot_1.png exists (checked straight away, to know whether to show the logo)
    bool counted;               // count is known (looked up once the selection settles)
    int count;                  // screenshots this game has
    int shown_index;            // 1-based number of the one shown (0: none yet)
    surface_t *shown;
    uint32_t shown_ms;          // when the shown one appeared
    uint32_t appeared_ms;       // when the first one appeared (it fades in from then)
    bool decoding;
    int generation;             // bumped on every game change, to drop stale decodes
} shots;
static bool screenshots_enabled = true;


static void screenshots_reset (void) {
    if (shots.decoding) {
        png_decoder_abort();
    }
    if (shots.shown) {
        rspq_wait();    // the RDP may still be drawing it
        surface_free(shots.shown);
        free(shots.shown);
    }
    int generation = shots.generation + 1;
    memset(&shots, 0, sizeof(shots));
    shots.generation = generation;
}

/** @brief Drop the loaded / loading screenshot but keep the selected game, so it can load again. */
static void screenshots_release (void) {
    if (shots.decoding) {
        png_decoder_abort();
        shots.decoding = false;
    }
    if (shots.shown) {
        rspq_wait();
        surface_free(shots.shown);
        free(shots.shown);
        shots.shown = NULL;
    }
    shots.shown_index = 0;
    shots.counted = false;
    shots.count = 0;
    shots.generation++;
}

/** @brief ".../screenshot_" for a game: where its screenshots are, the number and ".png" to follow. */
static void screenshot_base (const char *rom_path, const char game_code[4], char *out, size_t out_size) {
    rom_info_metadata_path(rom_path, game_code, out, out_size);
    char *name = strrchr(out, '/');
    if (name) {
        snprintf(name + 1, out_size - (size_t) (name + 1 - out), "screenshot_");
    } else {
        out[0] = '\0';
    }
}

/** @brief Note where the selected game's screenshots are; whether it has any is cached (the full count and loading wait for the carousel to settle). */
static void screenshots_select (const char *rom_path, const char game_code[4], bool has_any) {
    screenshot_base(rom_path, game_code, shots.base, sizeof(shots.base));
    shots.has_any = has_any && (shots.base[0] != '\0');
}

/** @brief How many screenshots the game has: screenshot_1.png, screenshot_2.png, ... */
static void screenshots_count (void) {
    shots.counted = true;
    for (shots.count = 0; shots.base[0] != '\0' && shots.count < SCREENSHOT_MAX; shots.count++) {
        char path[280];
        snprintf(path, sizeof(path), "%s%d.png", shots.base, shots.count + 1);
        if (!file_exists(path)) {
            break;
        }
    }
}

static void screenshot_decoded (png_err_t err, surface_t *image, void *data) {
    int generation = (int) (intptr_t) data;
    shots.decoding = false;
    if (err != PNG_OK || !image) {
        return;
    }
    if (generation != shots.generation) {
        surface_free(image);
        free(image);
        return;
    }
    if (shots.shown) {
        rspq_wait();
        surface_free(shots.shown);
        free(shots.shown);
    } else {
        shots.appeared_ms = get_ticks_ms();
    }
    shots.shown = image;
    shots.shown_index = (shots.shown_index % shots.count) + 1;
    shots.shown_ms = get_ticks_ms();
}

/** @brief Once the selection settles, count the screenshots and decode the first; then one per cycle. */
static void screenshots_update (void) {
    uint32_t now = get_ticks_ms();
    bool settled = ui_components_carousel_still_ms() >= SCREENSHOT_SETTLE_MS;
    if (!shots.counted) {
        if (!settled) {
            return;
        }
        screenshots_count();    // reads the SD card, so not while scrolling past games
    }
    if (shots.count == 0 || shots.decoding) {
        return;
    }
    bool due = shots.shown
        ? (shots.count > 1 && (now - shots.shown_ms) >= SCREENSHOT_CYCLE_MS)
        : settled;
    if (!due) {
        return;
    }
    char path[280];
    snprintf(path, sizeof(path), "%s%d.png", shots.base, (shots.shown_index % shots.count) + 1);
    if (png_decoder_start(path, GAME_INFO_SCREENSHOT_WIDTH, GAME_INFO_SCREENSHOT_HEIGHT,
            screenshot_decoded, (void *) (intptr_t) shots.generation) == PNG_OK) {
        shots.decoding = true;
    } else if (!shots.shown) {
        shots.count = 0;    // can't be read: leave the space empty
    }
}


static uint32_t hash_path (const char *path) {
    uint32_t hash = 2166136261u;     // FNV-1a
    for (const char *c = path; *c; c++) {
        hash = (hash ^ (uint8_t) (*c)) * 16777619u;
    }
    return hash;
}

static void cache_init (void) {
    if (cache.entries) {
        return;
    }
    cache.capacity = is_memory_expanded() ? INFO_CACHE_SIZE : INFO_CACHE_SIZE_SMALL;
    cache.entries = calloc(cache.capacity, sizeof(game_summary_t));
    for (int i = 0; i < INFO_CACHE_BUCKETS; i++) {
        cache.buckets[i] = -1;
    }
}

static game_summary_t *cache_find (const char *path) {
    if (!cache.entries) {
        return NULL;
    }
    uint32_t hash = hash_path(path);
    for (int i = cache.buckets[hash % INFO_CACHE_BUCKETS]; i >= 0; i = cache.entries[i].next) {
        if (cache.entries[i].hash == hash && strcmp(cache.entries[i].path, path) == 0) {
            return &cache.entries[i];
        }
    }
    return NULL;
}

/** @brief Take a slot out of its bucket and free it. */
static void cache_remove (int index) {
    game_summary_t *entry = &cache.entries[index];
    if (!entry->path) {
        return;
    }
    int *link = &cache.buckets[entry->hash % INFO_CACHE_BUCKETS];
    while (*link >= 0 && *link != index) {
        link = &cache.entries[*link].next;
    }
    if (*link == index) {
        *link = entry->next;
    }
    free(entry->path);
    free(entry->description);
    entry->path = NULL;
    entry->description = NULL;
}

/** @brief A slot for a new game: unused, or the oldest one. */
static game_summary_t *cache_add (const char *path) {
    cache_init();
    int index;
    if (cache.used < cache.capacity) {
        index = cache.used++;
    } else {
        index = cache.evict;
        cache.evict = (cache.evict + 1) % cache.capacity;
        cache_remove(index);
    }
    game_summary_t *entry = &cache.entries[index];
    memset(entry, 0, sizeof(*entry));
    entry->path = strdup(path);
    entry->hash = hash_path(path);
    int *bucket = &cache.buckets[entry->hash % INFO_CACHE_BUCKETS];
    entry->next = *bucket;
    *bucket = index;
    return entry;
}

static void copy_meta (char *out, size_t size, const char *value);
static const char *meta_value (const char *value);

/** @brief Read a game's info from the SD card into the cache. */
static game_summary_t *summary_load (path_t *path, entry_type_t type) {
    game_summary_t *summary = cache_add(path_get(path));
    if (!summary->path) {
        return NULL;
    }

    struct stat st;
    if (stat(path_get(path), &st) == 0) {
        summary->added = st.st_mtime;
        summary->size = st.st_size;
    }
    sound_poll();

    if (type == ENTRY_TYPE_ROM) {
        rom_info_t info;
        summary->is_rom = (rom_config_load(path, &info) == ROM_OK);
        sound_poll();
        if (summary->is_rom) {
            memcpy(summary->game_code, info.game_code, sizeof(summary->game_code));
            summary->version = info.version;
            summary->players = (uint8_t) MIN(info.meta.num_players, 255);
            summary->region = info.destination_code;
            summary->rumble_pak = info.features.rumble_pak;
            summary->controller_pak = info.features.controller_pak;
            summary->transfer_pak = info.features.transfer_pak;
            summary->vru = info.features.voice_recognition_unit;
            summary->expansion_pak = info.features.expansion_pak;
            summary->save_type = rom_info_get_save_type(&info);
            summary->cic = rom_info_get_cic_type(&info);
            summary->tv = rom_info_get_tv_type(&info);
            summary->cheats = info.settings.cheats_enabled;
            summary->clear_rdram = info.settings.clear_rdram_enabled;
            copy_meta(summary->developer, sizeof(summary->developer), info.meta.author);
            copy_meta(summary->publisher, sizeof(summary->publisher), info.meta.publisher);
            copy_meta(summary->release, sizeof(summary->release), info.meta.release_date);
            if (is_memory_expanded()) {
                const char *description = meta_value(info.meta.short_description);
                summary->description = description ? strdup(description) : NULL;
                summary->has_description = true;
            }
            rom_info_free_meta(&info);

            char first[280];
            screenshot_base(path_get(path), summary->game_code, first, sizeof(first) - 8);
            if (first[0] != '\0') {
                strcat(first, "1.png");
                summary->has_screenshot = file_exists(first);
            }
        }
    }
    return summary;
}

/** @brief Forget the selected entry: its screenshots and About page text. */
static void current_free (void) {
    screenshots_reset();
    if (current.about_loaded && current.about_is_rom) {
        rom_info_free_meta(&current.about);
    }
    free(current.path);
    memset(&current, 0, sizeof(current));
}

/** @brief Follow the selection: when it changes, drop the last entry's screenshots and About text. */
static void current_select (const char *path) {
    if (current.path && strcmp(current.path, path) == 0) {
        return;
    }
    current_free();
    current.path = strdup(path);
    about_scroll = 0;
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

/** @brief Copy a metadata string into the cache ("" when missing). */
static void copy_meta (char *out, size_t size, const char *value) {
    value = meta_value(value);
    snprintf(out, size, "%s", value ? value : "");
}

/** @brief A cached string, or NULL ("-") when empty. */
static const char *cached_text (const char *text) {
    return (text[0] != '\0') ? text : NULL;
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

static int tag_width (const char *text) {
    int nbytes = strlen(text);
    rdpq_paragraph_t *layout = rdpq_paragraph_build(&(rdpq_textparms_t) { 0 }, GAME_INFO_FONT, text, &nbytes);
    int width = (int) (layout->advance_x);
    rdpq_paragraph_free(layout);
    return width;
}

/** @brief One RMB PAK / USA style tag (plain value text). Returns its width, or -1 if it doesn't fit. */
static int draw_tag (int x, int y, const char *text) {
    int width = tag_width(text);
    if (x + width > value_right) {
        return -1;
    }
    render_paragraph_with_shadow((rdpq_textparms_t) { .style_id = STL_DEFAULT }, x, y, text, STL_SHADOW);
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
    // The icons are 7px tall (rows y-8 to y-2, centred on the text's capitals); the badge fits
    // them with BADGE_PADDING all round, its corners rounded by a 2px step.
    int width = (BADGE_PADDING * 2) + (MAX_PLAYERS * 7) - 2;
    int top = y - 8 - BADGE_PADDING;
    int bottom = y - 1 + BADGE_PADDING;
    ui_components_box_draw(x + 2, top, x + width - 2, bottom, GAME_INFO_BADGE_COLOR);
    ui_components_box_draw(x + 1, top + 1, x + width - 1, bottom - 1, GAME_INFO_BADGE_COLOR);
    ui_components_box_draw(x, top + 2, x + width, bottom - 2, GAME_INFO_BADGE_COLOR);
    for (uint32_t i = 0; i < MAX_PLAYERS; i++) {
        draw_player_icon(x + BADGE_PADDING + (i * 7), y - 8, (i < players) ? GAME_INFO_PLAYER_ON_COLOR : GAME_INFO_PLAYER_OFF_COLOR);
    }
}

static void draw_accessories (int x, int y, game_summary_t *info) {
    const char *badges[6];
    int count = 0;

    if (info->rumble_pak) badges[count++] = "RMB PAK";
    if (info->controller_pak) badges[count++] = "CTL PAK";
    if (info->transfer_pak) badges[count++] = "TFR PAK";
    if (info->expansion_pak == EXPANSION_PAK_REQUIRED) badges[count++] = "EXP PAK";
    else if (info->expansion_pak == EXPANSION_PAK_RECOMMENDED) badges[count++] = "EXP PAK+";
    if (info->vru) badges[count++] = "VRU";

    if (count == 0) {
        draw_text(x, y, STL_DEFAULT, "-");
        return;
    }
    for (int i = 0; i < count; i++) {
        if (x + tag_width(badges[i]) > value_right) {
            break;      // the rest don't fit (and no separator before them)
        }
        if (i > 0) {
            // Square separator, centred in the gap and on the capitals.
            int sx = x - (TAG_GAP / 2) - (TAG_SEPARATOR_SIZE / 2);
            int sy = y - (fonts_cap_height(GAME_INFO_FONT) / 2) - (TAG_SEPARATOR_SIZE / 2);
            ui_components_box_draw(sx, sy, sx + TAG_SEPARATOR_SIZE, sy + TAG_SEPARATOR_SIZE, TEXT_SECONDARY_COLOR);
        }
        x += draw_tag(x, y, badges[i]) + TAG_GAP;
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
/**
 * @brief The game's screenshots in the right half of the info area (Overview and Details pages),
 *        if it has any; values then stop short of them.
 */
/** @brief Opacity this long into a GAME_INFO_FADE_MS fade-in. */
static uint8_t fade_in_alpha (uint32_t since) {
    return (since >= GAME_INFO_FADE_MS) ? 0xFF : (uint8_t) ((since * 0xFF) / GAME_INFO_FADE_MS);
}

/**
 * @brief Opacity of the screenshot area's contents: hidden while the carousel slides and for
 *        GAME_INFO_FADE_DELAY_MS after, then faded in.
 */
static uint8_t settled_alpha (void) {
    uint32_t still = ui_components_carousel_still_ms();
    return (still < GAME_INFO_FADE_DELAY_MS) ? 0 : fade_in_alpha(still - GAME_INFO_FADE_DELAY_MS);
}

/** @brief A filled box at an opacity. */
static void draw_box_faded (int x0, int y0, int x1, int y1, color_t color, uint8_t alpha) {
    if (alpha == 0xFF) {
        ui_components_box_draw(x0, y0, x1, y1, color);
        return;
    }
    color.a = alpha;
    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_set_prim_color(color);
        rdpq_fill_rectangle(x0, y0, x1, y1);
    rdpq_mode_pop();
}

/** @brief The spinning N64 logo, centred in the screenshot area, faded in once the carousel settles. */
static void draw_logo (void) {
    uint8_t alpha = settled_alpha();
    if (alpha == 0) {
        return;
    }
    ui_components_n64_logo_draw(
        GAME_INFO_SCREENSHOT_X + ((GAME_INFO_SCREENSHOT_WIDTH - N64_LOGO_WIDTH) / 2),
        GAME_INFO_SCREENSHOT_Y + ((GAME_INFO_SCREENSHOT_HEIGHT - N64_LOGO_HEIGHT) / 2),
        ui_components_n64_logo_frame(), alpha);
}

static void draw_screenshots (entry_t *entry) {
    if (!entry || entry->type != ENTRY_TYPE_ROM) {
        return;
    }
    if (screenshots_enabled) {
        screenshots_update();
    }
    value_right = GAME_INFO_SCREENSHOT_X - GAME_INFO_SCREENSHOT_GAP;
    // Gallery off or no screenshots for this game: the logo. Screenshots still loading: nothing yet.
    if (!screenshots_enabled || !shots.has_any || (shots.counted && shots.count == 0)) {
        draw_logo();
        return;
    }
    if (!shots.shown) {
        return;
    }
    // Like the logo, faded in once the carousel settles, or once the first one loads if that's later.
    uint8_t alpha = MIN(settled_alpha(), fade_in_alpha(get_ticks_ms() - shots.appeared_ms));
    if (alpha == 0) {
        return;
    }
    {
        // Drop shadow, then a white outline, then the screenshot.
        int x0 = GAME_INFO_SCREENSHOT_X - GAME_INFO_SCREENSHOT_OUTLINE;
        int y0 = GAME_INFO_SCREENSHOT_Y - GAME_INFO_SCREENSHOT_OUTLINE;
        int x1 = GAME_INFO_SCREENSHOT_X + GAME_INFO_SCREENSHOT_WIDTH + GAME_INFO_SCREENSHOT_OUTLINE;
        int y1 = GAME_INFO_SCREENSHOT_Y + GAME_INFO_SCREENSHOT_HEIGHT + GAME_INFO_SCREENSHOT_OUTLINE;
        int s = GAME_INFO_SCREENSHOT_SHADOW;
        draw_box_faded(x0 + s, y0 + s, x1 + s, y1 + s, GAME_INFO_SCREENSHOT_SHADOW_COLOR, alpha);
        draw_box_faded(x0, y0, x1, y1, GAME_INFO_SCREENSHOT_OUTLINE_COLOR, alpha);
        rdpq_mode_push();
            if (alpha == 0xFF) {
                rdpq_set_mode_copy(false);
            } else {
                rdpq_set_mode_standard();
                rdpq_mode_combiner(RDPQ_COMBINER1((0, 0, 0, TEX0), (0, 0, 0, PRIM)));
                rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
                rdpq_set_prim_color(RGBA32(0xFF, 0xFF, 0xFF, alpha));
            }
            rdpq_tex_blit(shots.shown, GAME_INFO_SCREENSHOT_X, GAME_INFO_SCREENSHOT_Y, NULL);
        rdpq_mode_pop();
    }
}

static void draw_overview_page (entry_t *entry, game_summary_t *summary, time_t last_played) {
    game_summary_t *info = summary->is_rom ? summary : NULL;
    int x = GAME_INFO_VALUE_X;
    int y = GAME_INFO_Y;
    char date[32];

    draw_screenshots(entry);

    draw_row(y, "Player Count");
    draw_player_count(x, y, info ? info->players : 0);
    y += GAME_INFO_ROW_PITCH;

    draw_row(y, "Accessories");
    if (info) {
        draw_accessories(x, y, info);
    } else {
        draw_value(y, NULL);
    }
    y += GAME_INFO_ROW_PITCH;

    draw_row(y, "Region");
    const char *region = info ? format_region(info->region) : NULL;
    if (region) {
        draw_tag(x, y, region);
    } else {
        draw_value(y, NULL);
    }
    y += GAME_INFO_ROW_PITCH + GAME_INFO_GROUP_GAP;

    draw_row(y, "Developers");
    draw_value(y, info ? cached_text(info->developer) : NULL);
    y += GAME_INFO_ROW_PITCH;

    draw_row(y, "Publishers");
    draw_value(y, info ? cached_text(info->publisher) : NULL);
    y += GAME_INFO_ROW_PITCH;

    draw_row(y, "Release");
    draw_value(y, info ? cached_text(info->release) : NULL);
    y += GAME_INFO_ROW_PITCH + GAME_INFO_GROUP_GAP;

    draw_row(y, "Added");
    draw_value(y, format_date(summary->added, date, sizeof(date)));
    y += GAME_INFO_ROW_PITCH;

    draw_row(y, "Last Played");
    draw_value(y, format_date(last_played, date, sizeof(date)));
}

/** @brief Page 2: header and boot details. */
static void draw_details_page (entry_t *entry, game_summary_t *info) {
    int y = GAME_INFO_Y;
    char buffer[32];

    draw_screenshots(entry);

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
    draw_value(y, format_size(info->size ? info->size : entry->size, buffer, sizeof(buffer)));
    y += GAME_INFO_ROW_PITCH + GAME_INFO_GROUP_GAP;

    draw_row(y, "Save Type");
    draw_value(y, format_save_type(info->save_type));
    y += GAME_INFO_ROW_PITCH;

    draw_row(y, "CIC");
    draw_value(y, format_cic(info->cic));
    y += GAME_INFO_ROW_PITCH;

    draw_row(y, "Video");
    draw_value(y, format_video(info->tv));
    y += GAME_INFO_ROW_PITCH + GAME_INFO_GROUP_GAP;

    draw_row(y, "Cheats");
    draw_value(y, info->cheats ? "On" : "Off");
    y += GAME_INFO_ROW_PITCH;

    draw_row(y, "Clear RDRAM");
    draw_value(y, info->clear_rdram ? "On" : "Off");
}

/** @brief Page 3: the game's description (NULL: none), using the whole info area; scrolls when it doesn't fit. */
static void draw_about_page (const char *description) {
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
/**
 * @brief Turn the screenshot gallery on or off (off: none are loaded or shown).
 */
void ui_components_game_info_screenshots_enable (bool enabled) {
    if (!enabled && screenshots_enabled) {
        screenshots_release();
    }
    screenshots_enabled = enabled;
}

void ui_components_game_info_invalidate (void) {
    current_free();
    if (cache.entries) {
        for (int i = 0; i < cache.used; i++) {
            free(cache.entries[i].path);
            free(cache.entries[i].description);
        }
        free(cache.entries);
    }
    memset(&cache, 0, sizeof(cache));
    free(prefetch.directory);
    memset(&prefetch, 0, sizeof(prefetch));
}

void ui_components_game_info_forget (path_t *path) {
    game_summary_t *summary = path ? cache_find(path_get(path)) : NULL;
    if (summary) {
        cache_remove((int) (summary - cache.entries));
    }
    if (path && current.path && strcmp(current.path, path_get(path)) == 0) {
        current_free();
    }
}

void ui_components_game_info_prefetch (path_t *directory, entry_t *list, int32_t count, int32_t selected, uint32_t budget_us) {
    if (!list || count <= 0) {
        return;
    }
    // A different list (another folder, or Faves / History): start again, outward from the selection.
    const char *dir = directory ? path_get(directory) : "";
    uint32_t signature = hash_path(list[0].name) ^ (hash_path(list[count - 1].name) * 31u);
    if (prefetch.list != list || prefetch.count != count || prefetch.signature != signature ||
            !prefetch.directory || strcmp(prefetch.directory, dir) != 0) {
        free(prefetch.directory);
        prefetch.list = list;
        prefetch.count = count;
        prefetch.signature = signature;
        prefetch.directory = strdup(dir);
        prefetch.origin = (selected >= 0 && selected < count) ? selected : 0;
        prefetch.done = 0;
    }

    uint64_t start = get_ticks_us();
    while (prefetch.done < count && (get_ticks_us() - start) < budget_us) {
        int32_t k = prefetch.done++;
        int32_t offset = (k % 2) ? ((k + 1) / 2) : -(k / 2);
        int32_t index = ((prefetch.origin + offset) % count + count) % count;
        entry_t *entry = &list[index];
        if (entry->type == ENTRY_TYPE_DIR) {
            continue;
        }
        path_t *path = directory ? path_clone_push(directory, entry->name) : path_create(entry->name);
        if (!cache_find(path_get(path))) {
            summary_load(path, entry->type);
        }
        path_free(path);
    }
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
    current_select(path_get(path));
    bool settled = ui_components_carousel_still_ms() >= GAME_INFO_LOAD_DELAY_MS;

    game_summary_t *summary = cache_find(path_get(path));
    if (summary && entry->size > 0 && summary->size > 0 && entry->size != summary->size) {
        // The file changed since it was cached.
        cache_remove((int) (summary - cache.entries));
        summary = NULL;
    }
    if (!summary) {
        if (!settled) {
            // Not cached yet: reading it takes several SD card reads, so while the carousel slides
            // past games the panel stays empty until it settles.
            path_free(path);
            return;
        }
        summary = summary_load(path, entry->type);
        if (!summary) {
            path_free(path);
            return;
        }
    }
    if (summary->is_rom && !current.shots_selected) {
        screenshots_select(path_get(path), summary->game_code, summary->has_screenshot);
        current.shots_selected = true;
    }

    if (page != last_page_drawn) {
        about_scroll = 0;
    }
    last_page_drawn = page;
    value_right = VISIBLE_AREA_X1;     // the Overview page narrows it for the screenshot

    if (page == 1 && summary->is_rom) {
        draw_details_page(entry, summary);
    } else if (page == PAGE_ABOUT && summary->is_rom) {
        if (summary->has_description) {
            draw_about_page(summary->description);
        } else {
            // Not cached (no Expansion Pak): read the full info once the carousel settles.
            if (!current.about_loaded && settled) {
                current.about_loaded = true;
                current.about_is_rom = (rom_config_load(path, &current.about) == ROM_OK);
                sound_poll();
            }
            if (current.about_loaded && current.about_is_rom) {
                draw_about_page(meta_value(current.about.meta.short_description));
            }
        }
    } else {
        draw_overview_page(entry, summary, bookkeeping_history_last_played(bookkeeping, path));
    }
    path_free(path);
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
