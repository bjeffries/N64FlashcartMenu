/**
 * @file system_info.c
 * @brief N64 Information screen (Settings tab): region, memory, 64DD and controllers
 * @ingroup view
 */

#include <time.h>

#include "../cart_load.h"
#include "../sound.h"
#include "../ui_components/constants.h"
#include "views.h"


static int joypad[4];
static int accessory[4];


static const char *region_value (menu_t *menu) {
    switch (get_tv_type()) {
        case TV_NTSC: return "NTSC";
        case TV_PAL: return "PAL";
        case TV_MPAL: return "MPAL";
        default: return "Unknown";
    }
}

static const char *expansion_value (menu_t *menu) {
    return is_memory_expanded() ? "Installed (8 MB)" : "Not installed (4 MB)";
}

static const char *disk_drive_value (menu_t *menu) {
    return is_64dd_connected() ? "Connected" : "Not connected";
}

static const char *controller_value (int port) {
    if (!joypad[port]) {
        return "Not connected";
    }
    switch (accessory[port]) {
        case JOYPAD_ACCESSORY_TYPE_RUMBLE_PAK: return "Connected, Rumble Pak";
        case JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK: return "Connected, Controller Pak";
        case JOYPAD_ACCESSORY_TYPE_TRANSFER_PAK: return "Connected, Transfer Pak";
        case JOYPAD_ACCESSORY_TYPE_BIO_SENSOR: return "Connected, Bio Sensor";
        case JOYPAD_ACCESSORY_TYPE_SNAP_STATION: return "Connected, Snap Station";
        case JOYPAD_ACCESSORY_TYPE_NONE: return "Connected";
        default: return "Connected, unknown accessory";
    }
}

static const char *controller_1 (menu_t *menu) { return controller_value(0); }
static const char *controller_2 (menu_t *menu) { return controller_value(1); }
static const char *controller_3 (menu_t *menu) { return controller_value(2); }
static const char *controller_4 (menu_t *menu) { return controller_value(3); }

static option_t options[] = {
    { .label = "Region", .type = OPTION_INFO, .value = region_value },
    { .label = "Expansion Pak", .type = OPTION_INFO, .value = expansion_value,
      .description = "Some games, and cheat codes, need the Expansion Pak." },
    { .label = "64DD Drive", .type = OPTION_INFO, .value = disk_drive_value,
      .description = "A real 64DD disk drive attached under the console." },
    { .label = "Controller 1", .type = OPTION_INFO, .value = controller_1 },
    { .label = "Controller 2", .type = OPTION_INFO, .value = controller_2 },
    { .label = "Controller 3", .type = OPTION_INFO, .value = controller_3 },
    { .label = "Controller 4", .type = OPTION_INFO, .value = controller_4 },
};

static option_list_t list = {
    .options = options,
    .count = sizeof(options) / sizeof(options[0]),
};


static void process (menu_t *menu) {
    JOYPAD_PORT_FOREACH (port) {
        joypad[port] = (joypad_get_style(port) != JOYPAD_STYLE_NONE);
        accessory[port] = joypad_get_accessory_type(port);
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

    ui_components_option_screen_draw(menu, "N64 Information", &list);

    rdpq_detach_show();
}


void view_system_info_init (menu_t *menu) {
    ui_components_option_list_init(&list);
}

void view_system_info_display (menu_t *menu, surface_t *display) {
    process(menu);

    draw(menu, display);
}
