/**
 * @file settings_editor.c
 * @brief Menu Settings screen (Settings tab): an option list of the menu's settings
 * @ingroup view
 */

#include <stdbool.h>
#include "../sound.h"
#include "../settings.h"
#include "../ui_components/constants.h"
#include "views.h"

static bool confirm_reset = false;


static void save_and_reload_library (menu_t *menu) {
    settings_save(&menu->settings);
    menu->browser.reload = true;
}

static bool get_soundfx (menu_t *menu) { return menu->settings.soundfx_enabled; }
static void set_soundfx (menu_t *menu, bool value) {
    menu->settings.soundfx_enabled = value;
    sound_use_sfx(value);
    settings_save(&menu->settings);
}

static bool get_boot_animation (menu_t *menu) { return menu->settings.boot_animation_enabled; }
static void set_boot_animation (menu_t *menu, bool value) {
    menu->settings.boot_animation_enabled = value;
    settings_save(&menu->settings);
}

static bool get_hidden_games (menu_t *menu) { return menu->settings.show_hidden_games; }
static void set_hidden_games (menu_t *menu, bool value) {
    menu->settings.show_hidden_games = value;
    save_and_reload_library(menu);
}

static bool get_use_saves_folder (menu_t *menu) { return menu->settings.use_saves_folder; }
static void set_use_saves_folder (menu_t *menu, bool value) {
    menu->settings.use_saves_folder = value;
    settings_save(&menu->settings);
}

static bool get_show_saves_folder (menu_t *menu) { return menu->settings.show_saves_folder; }
static void set_show_saves_folder (menu_t *menu, bool value) {
    menu->settings.show_saves_folder = value;
    save_and_reload_library(menu);
}

static bool get_hidden_files (menu_t *menu) { return menu->settings.show_protected_entries; }
static void set_hidden_files (menu_t *menu, bool value) {
    menu->settings.show_protected_entries = value;
    save_and_reload_library(menu);
}

#ifdef FEATURE_AUTOLOAD_ROM_ENABLED
static bool get_loading_bar (menu_t *menu) { return menu->settings.loading_progress_bar_enabled; }
static void set_loading_bar (menu_t *menu, bool value) {
    menu->settings.loading_progress_bar_enabled = value;
    settings_save(&menu->settings);
}
#else
static bool get_fast_reboot (menu_t *menu) { return menu->settings.rom_fast_reboot_enabled; }
static void set_fast_reboot (menu_t *menu, bool value) {
    menu->settings.rom_fast_reboot_enabled = value;
    settings_save(&menu->settings);
}
#endif

static bool get_pal60 (menu_t *menu) { return menu->settings.pal60_enabled; }
static void set_pal60 (menu_t *menu, bool value) {
    // PAL60 only exists on PAL consoles; switch the video timing immediately, no reboot needed.
    if (get_tv_type() != TV_PAL) {
        value = false;
        menu_show_error(menu, "PAL60 is only available on PAL consoles");
    } else {
        vi_set_timing_preset(value ? &VI_TIMING_PAL60 : &VI_TIMING_PAL);
    }
    menu->settings.pal60_enabled = value;
    settings_save(&menu->settings);
}

#ifdef BETA_SETTINGS
static bool get_file_extensions (menu_t *menu) { return menu->settings.show_browser_file_extensions; }
static void set_file_extensions (menu_t *menu, bool value) {
    menu->settings.show_browser_file_extensions = value;
    save_and_reload_library(menu);
}

static bool get_rom_tags (menu_t *menu) { return menu->settings.show_browser_rom_tags; }
static void set_rom_tags (menu_t *menu, bool value) {
    menu->settings.show_browser_rom_tags = value;
    settings_save(&menu->settings);
}

static bool get_rumble (menu_t *menu) { return menu->settings.rumble_enabled; }
static void set_rumble (menu_t *menu, bool value) {
    menu->settings.rumble_enabled = value;
    settings_save(&menu->settings);
}
#endif

static const char *start_folder_value (menu_t *menu) {
    const char *folder = menu->settings.default_directory;
    return (folder && folder[0] != '\0') ? folder : "/";
}

static void reset_start_folder (menu_t *menu) {
    free(menu->settings.default_directory);
    menu->settings.default_directory = strdup("/");
    settings_save(&menu->settings);
}

static void ask_reset (menu_t *menu) {
    confirm_reset = true;
}

static option_t options[] = {
    { .label = "Boot Animation", .type = OPTION_TOGGLE, .get = get_boot_animation, .set = set_boot_animation,
      .description = "Play the Eclipse Cart animation when the console is switched on." },
    { .label = "Sound Effects", .type = OPTION_TOGGLE, .get = get_soundfx, .set = set_soundfx,
      .description = "Menu sounds when moving and selecting." },
    { .label = "Show Hidden Games", .type = OPTION_TOGGLE, .get = get_hidden_games, .set = set_hidden_games,
      .description = "List games you hid with C-Down in the Library, grayed out, so you can unhide them." },
    { .label = "Use Saves Folder", .type = OPTION_TOGGLE, .get = get_use_saves_folder, .set = set_use_saves_folder,
      .description = "Keep save files in a \"saves\" folder next to each game instead of beside it." },
    { .label = "Show Saves Folder", .type = OPTION_TOGGLE, .get = get_show_saves_folder, .set = set_show_saves_folder,
      .description = "List \"saves\" folders in the Library." },
    { .label = "Show Hidden Files", .type = OPTION_TOGGLE, .get = get_hidden_files, .set = set_hidden_files,
      .description = "List the menu's own folder and system folders in the Library." },
#ifdef FEATURE_AUTOLOAD_ROM_ENABLED
    { .label = "Loading Bar", .type = OPTION_TOGGLE, .get = get_loading_bar, .set = set_loading_bar,
      .description = "Show progress while a game loads." },
#else
    { .label = "Fast Reboot", .type = OPTION_TOGGLE, .get = get_fast_reboot, .set = set_fast_reboot,
      .description = "Pressing Reset restarts the current game instead of returning to this menu." },
#endif
    { .label = "PAL60 Mode", .type = OPTION_TOGGLE, .get = get_pal60, .set = set_pal60,
      .description = "PAL consoles only: 60Hz output. If your TV goes dark, turn it off in menu/config.ini." },
#ifdef BETA_SETTINGS
    { .label = "Hide File Extensions", .type = OPTION_TOGGLE, .get = get_file_extensions, .set = set_file_extensions },
    { .label = "Hide ROM Tags", .type = OPTION_TOGGLE, .get = get_rom_tags, .set = set_rom_tags },
    { .label = "Rumble Feedback", .type = OPTION_TOGGLE, .get = get_rumble, .set = set_rumble },
#endif
    { .label = "Start Folder", .type = OPTION_ACTION, .value = start_folder_value,
      .action = reset_start_folder, .action_name = "Reset",
      .description = "Folder the Library opens in. Set it with C-Left on a folder; A resets it to the top." },
    { .label = "Reset Settings", .type = OPTION_ACTION, .action = ask_reset,
      .description = "Put every menu setting back to its default." },
};

static option_list_t list = {
    .options = options,
    .count = sizeof(options) / sizeof(options[0]),
};


static void process (menu_t *menu) {
    if (confirm_reset) {
        if (menu->actions.enter) {
            settings_reset_to_defaults();
            menu_show_error(menu, "Settings reset.\nRestart the N64 for them to take effect.");
            confirm_reset = false;
            sound_play_effect(SFX_SETTING);
        } else if (menu->actions.back) {
            confirm_reset = false;
            sound_play_effect(SFX_EXIT);
        }
        return;
    }

    if (ui_components_option_list_process(menu, &list)) {
        return;
    }

    if (menu->actions.back) {
        menu->next_mode = MENU_MODE_SETTINGS_HUB;
        sound_play_effect(SFX_EXIT);
    }
}

static void draw (menu_t *menu, surface_t *d) {
    rdpq_attach_clear(d, NULL);

    ui_components_option_screen_draw(menu, "Menu Settings", &list);

    if (confirm_reset) {
        ui_components_messagebox_draw(
            "Reset all menu settings?\n\n"
            "A: Reset    B: Cancel"
        );
    }

    rdpq_detach_show();
}


void view_settings_init (menu_t *menu) {
    confirm_reset = false;
    ui_components_option_list_init(&list);
}

void view_settings_display (menu_t *menu, surface_t *display) {
    process(menu);

    draw(menu, display);
}
