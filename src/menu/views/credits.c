/**
 * @file credits.c
 * @brief Menu Information screen (Settings tab): version, credits and licenses
 * @ingroup view
 */

#include "views.h"
#include "../sound.h"
#include "../ui_components/constants.h"

#ifndef MENU_VERSION
#define MENU_VERSION "Unknown"
#endif

#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP "Unknown"
#endif

static sys_version_t sdk_version = {0};
static bool show_libraries = false;


static const char *version_value (menu_t *menu) { return MENU_VERSION; }
static const char *built_value (menu_t *menu) { return BUILD_TIMESTAMP; }

static const char *libdragon_value (menu_t *menu) {
    static char buffer[64];
    snprintf(buffer, sizeof(buffer), "%s%s (%s)", sdk_version.branch, sdk_version.dirty ? "*" : "", sdk_version.commit_date);
    return buffer;
}

static const char *authors_value (menu_t *menu) { return "Robin Jones, Mateusz Faderewski"; }
static const char *license_value (menu_t *menu) { return "AGPL-3.0"; }
static const char *font_value (menu_t *menu) { return "Analogue OS by AbFarid"; }
static const char *labels_value (menu_t *menu) { return "Analogue 3D labels.db"; }

static void open_libraries (menu_t *menu) {
    show_libraries = true;
}

static option_t options[] = {
    { .label = "Version", .type = OPTION_INFO, .value = version_value,
      .description = "This menu is a customized build of N64FlashcartMenu for the SummerCart64." },
    { .label = "Built", .type = OPTION_INFO, .value = built_value },
    { .label = "libdragon", .type = OPTION_INFO, .value = libdragon_value,
      .description = "The open source N64 SDK this menu is built with." },
    { .label = "Authors", .type = OPTION_INFO, .value = authors_value,
      .description = "N64FlashcartMenu by NetworkFusion and Polprzewodnikowy, with thanks to every project contributor." },
    { .label = "License", .type = OPTION_INFO, .value = license_value,
      .description = "Source: github.com/Polprzewodnikowy/N64FlashcartMenu" },
    { .label = "Font", .type = OPTION_INFO, .value = font_value,
      .description = "github.com/AbFarid/analogue-os-font (SIL Open Font License 1.1)" },
    { .label = "Labels", .type = OPTION_INFO, .value = labels_value,
      .description = "Cartridge label art is read from menu/labels.db on the SD card." },
    { .label = "Libraries", .type = OPTION_ACTION, .action = open_libraries,
      .description = "Open source libraries used by the menu." },
};

static option_list_t list = {
    .options = options,
    .count = sizeof(options) / sizeof(options[0]),
};


static void process (menu_t *menu) {
    if (show_libraries) {
        if (menu->actions.enter || menu->actions.back) {
            show_libraries = false;
            sound_play_effect(SFX_EXIT);
        }
        return;
    }

    if (ui_components_option_list_process(menu, &list)) {
        return;
    }

    if (menu->actions.back) {
        sound_play_effect(SFX_EXIT);
        menu->next_mode = MENU_MODE_SETTINGS_HUB;
    }
}

static void draw (menu_t *menu, surface_t *d) {
    rdpq_attach_clear(d, NULL);

    ui_components_option_screen_draw(menu, "Menu Information", &list);

    if (show_libraries) {
        ui_components_messagebox_draw(
            "Open source libraries\n\n"
            "libdragon (Unlicense)\n"
            "libspng (BSD 2-Clause)\n"
            "miniz (MIT)\n"
        );
    }

    rdpq_detach_show();
}


void view_credits_init (menu_t *menu) {
    sys_get_version(&sdk_version);
    show_libraries = false;
    ui_components_option_list_init(&list);
}

void view_credits_display (menu_t *menu, surface_t *display) {
    process(menu);

    draw(menu, display);
}
