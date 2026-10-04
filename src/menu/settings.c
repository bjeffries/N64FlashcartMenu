#include <libdragon.h>
#include "ini_parser.h"

#include "settings.h"
#include "utils/fs.h"


static char *settings_path = NULL;

static void load_colours (ini_t *ini, const char *key, const uint32_t defaults[5], uint32_t out[5]) {
    const char *text = ini_get_string(ini, "menu", key, "");
    for (int i = 0; i < 5; i++) {
        out[i] = defaults[i];
    }
    for (int i = 0; i < 5 && text && *text; i++) {
        char *end;
        unsigned long value = strtoul(text, &end, 16);
        if (end == text) {
            break;
        }
        out[i] = (uint32_t) (value & 0xFFFFFF);
        text = (*end == ',') ? end + 1 : end;
    }
}

static void save_colours (ini_t *ini, const char *key, const uint32_t colours[5]) {
    char text[64];
    snprintf(text, sizeof(text), "%06lX,%06lX,%06lX,%06lX,%06lX",
        (unsigned long) colours[0], (unsigned long) colours[1], (unsigned long) colours[2], (unsigned long) colours[3], (unsigned long) colours[4]);
    ini_set_string(ini, "menu", key, text);
}


static settings_t init = {
    .schema_revision = 1,
    .first_run = true,
    .pal60_enabled = false,
    .force_progressive_scan = false,
    .show_protected_entries = false,
    .show_hidden_games = false,
    .default_directory = SETTINGS_DEFAULT_DIRECTORY,  // falls back to "/" if missing (menu.c)
    .palette = "dusk",
    .custom_palettes = {
        { 0x000000, 0x1E1E1E, 0x404040, 0x808080, 0xFFFFFF },   // Monochrome
        { 0x000000, 0x1E1E1E, 0x404040, 0x808080, 0xFFFFFF },
    },
    .use_saves_folder = true,
    .show_saves_folder = false,
    .show_save_files = false,
    .show_cheat_files = false,
    .show_rom_configuration_files = false,
    .soundfx_enabled = false,
    .boot_animation_enabled = true,
    .screenshot_gallery_enabled = true,
    .controller_hints_enabled = true,
    .carousel_at_bottom = true,
    .screensaver_timeout = 60,
#ifdef FEATURE_AUTOLOAD_ROM_ENABLED
    .rom_autoload_enabled = false,
    .rom_autoload_path = "",
    .rom_autoload_filename = "",
    .loading_progress_bar_enabled = true,
#else
    .rom_fast_reboot_enabled = false,
#endif    
    /* Beta feature flags (should always init to default) */
    .show_browser_file_extensions = true,
    .show_browser_rom_tags = true,
    .wrap_file_list_scrolling = false,
    .rumble_enabled = false,
};


void settings_init (char *path) {
    if (settings_path) {
        free(settings_path);
    }
    settings_path = strdup(path);
}

void settings_load (settings_t *settings) {
    if (!file_exists(settings_path)) {
        settings_save(&init);
    }

    ini_t *ini = ini_try_load(settings_path);

    settings->schema_revision = ini_get_int(ini, "menu", "schema_revision", init.schema_revision);
    settings->first_run = ini_get_bool(ini, "menu", "first_run", init.first_run);
    settings->pal60_enabled = ini_get_bool(ini, "menu", "pal60", init.pal60_enabled);
    settings->force_progressive_scan = ini_get_bool(ini, "menu", "force_progressive_scan", init.force_progressive_scan);
    settings->show_protected_entries = ini_get_bool(ini, "menu", "show_protected_entries", init.show_protected_entries);
    settings->show_hidden_games = ini_get_bool(ini, "menu", "show_hidden_games", init.show_hidden_games);
    free(settings->default_directory);
    settings->default_directory = strdup(ini_get_string(ini, "menu", "default_directory", init.default_directory));
    free(settings->palette);
    settings->palette = strdup(ini_get_string(ini, "menu", "palette", init.palette));
    load_colours(ini, "custom1_colors", init.custom_palettes[0], settings->custom_palettes[0]);
    load_colours(ini, "custom2_colors", init.custom_palettes[1], settings->custom_palettes[1]);
    settings->use_saves_folder = ini_get_bool(ini, "menu", "use_saves_folder", init.use_saves_folder);
    settings->show_saves_folder = ini_get_bool(ini, "menu", "show_saves_folder", init.show_saves_folder);
    settings->show_save_files = ini_get_bool(ini, "menu", "show_save_files", init.show_save_files);
    settings->show_cheat_files = ini_get_bool(ini, "menu", "show_cheat_files", init.show_cheat_files);
    settings->show_rom_configuration_files = ini_get_bool(ini, "menu", "show_rom_configuration_files", init.show_rom_configuration_files);
    settings->soundfx_enabled = ini_get_bool(ini, "menu", "soundfx_enabled", init.soundfx_enabled);
    settings->boot_animation_enabled = ini_get_bool(ini, "menu", "boot_animation_enabled", init.boot_animation_enabled);
    settings->screenshot_gallery_enabled = ini_get_bool(ini, "menu", "screenshot_gallery_enabled", init.screenshot_gallery_enabled);
    settings->controller_hints_enabled = ini_get_bool(ini, "menu", "controller_hints_enabled", init.controller_hints_enabled);
    settings->carousel_at_bottom = ini_get_bool(ini, "menu", "carousel_at_bottom", init.carousel_at_bottom);
    settings->screensaver_timeout = ini_get_int(ini, "menu", "screensaver_timeout", init.screensaver_timeout);

#ifdef FEATURE_AUTOLOAD_ROM_ENABLED
    settings->rom_autoload_enabled = ini_get_bool(ini, "menu", "autoload_rom_enabled", init.rom_autoload_enabled);
    free(settings->rom_autoload_path);
    settings->rom_autoload_path = strdup(ini_get_string(ini, "autoload", "rom_path", init.rom_autoload_path));
    free(settings->rom_autoload_filename);
    settings->rom_autoload_filename = strdup(ini_get_string(ini, "autoload", "rom_filename", init.rom_autoload_filename));
    settings->loading_progress_bar_enabled = ini_get_bool(ini, "menu", "loading_progress_bar_enabled", init.loading_progress_bar_enabled);
#else
    settings->rom_fast_reboot_enabled = ini_get_bool(ini, "menu", "reboot_rom_enabled", init.rom_fast_reboot_enabled);
#endif
    /* Beta feature flags, they might not be in the file */
    settings->show_browser_file_extensions = ini_get_bool(ini, "menu", "show_browser_file_extensions", init.show_browser_file_extensions);
    settings->show_browser_rom_tags = ini_get_bool(ini, "menu", "show_browser_rom_tags", init.show_browser_rom_tags);
    settings->wrap_file_list_scrolling = ini_get_bool(ini, "menu", "wrap_file_list_scrolling", init.wrap_file_list_scrolling);
    settings->rumble_enabled = ini_get_bool(ini, "menu_beta_flag", "rumble_enabled", init.rumble_enabled);

    ini_free(ini);
}

void settings_save (settings_t *settings) {
    ini_t *ini = ini_create();

    ini_set_int(ini, "menu", "schema_revision", settings->schema_revision);
    ini_set_bool(ini, "menu", "first_run", settings->first_run);
    ini_set_bool(ini, "menu", "pal60", settings->pal60_enabled);
    ini_set_bool(ini, "menu", "force_progressive_scan", settings->force_progressive_scan);
    ini_set_bool(ini, "menu", "show_protected_entries", settings->show_protected_entries);
    ini_set_bool(ini, "menu", "show_hidden_games", settings->show_hidden_games);
    ini_set_string(ini, "menu", "default_directory", settings->default_directory);
    ini_set_string(ini, "menu", "palette", settings->palette);
    save_colours(ini, "custom1_colors", settings->custom_palettes[0]);
    save_colours(ini, "custom2_colors", settings->custom_palettes[1]);
    ini_set_bool(ini, "menu", "use_saves_folder", settings->use_saves_folder);
    ini_set_bool(ini, "menu", "show_saves_folder", settings->show_saves_folder);
    ini_set_bool(ini, "menu", "show_save_files", settings->show_save_files);
    ini_set_bool(ini, "menu", "show_cheat_files", settings->show_cheat_files);
    ini_set_bool(ini, "menu", "show_rom_configuration_files", settings->show_rom_configuration_files);
    ini_set_bool(ini, "menu", "soundfx_enabled", settings->soundfx_enabled);
    ini_set_bool(ini, "menu", "boot_animation_enabled", settings->boot_animation_enabled);
    ini_set_bool(ini, "menu", "screenshot_gallery_enabled", settings->screenshot_gallery_enabled);
    ini_set_bool(ini, "menu", "controller_hints_enabled", settings->controller_hints_enabled);
    ini_set_bool(ini, "menu", "carousel_at_bottom", settings->carousel_at_bottom);
    ini_set_int(ini, "menu", "screensaver_timeout", settings->screensaver_timeout);
#ifdef FEATURE_AUTOLOAD_ROM_ENABLED
    ini_set_bool(ini, "menu", "autoload_rom_enabled", settings->rom_autoload_enabled);
    ini_set_string(ini, "autoload", "rom_path", settings->rom_autoload_path);
    ini_set_string(ini, "autoload", "rom_filename", settings->rom_autoload_filename);
    ini_set_bool(ini, "menu", "loading_progress_bar_enabled", settings->loading_progress_bar_enabled);
#else
    ini_set_bool(ini, "menu", "reboot_rom_enabled", settings->rom_fast_reboot_enabled);
#endif

    /* Beta feature flags, they should not save until production ready! */
    // ini_set_bool(ini, "menu", "show_browser_file_extensions", settings->show_browser_file_extensions);
    // ini_set_bool(ini, "menu", "show_browser_rom_tags", settings->show_browser_rom_tags);
    ini_set_bool(ini, "menu", "wrap_file_list_scrolling", settings->wrap_file_list_scrolling);
    // ini_set_bool(ini, "menu_beta_flag", "rumble_enabled", settings->rumble_enabled);

    if (!ini_save(ini, settings_path)) {
        debugf("[SETTINGS] Failed to save settings to %s\n", settings_path);
    }

    ini_free(ini);
}

void settings_reset_to_defaults() {
    remove(settings_path);
}