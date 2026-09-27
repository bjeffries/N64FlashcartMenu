/**
 * @file settings_hub.c
 * @brief Settings tab: list of the menu's settings and information screens
 * @ingroup view
 */

#include "../fonts.h"
#include "../sound.h"
#include "../ui_components/constants.h"
#include "views.h"

static const struct {
    const char *name;
    const char *description;
    menu_mode_t mode;
} items[] = {
    { "Menu Settings", "Sound, saves folder, hidden files and video options", MENU_MODE_SETTINGS_EDITOR },
    { "Controller Pak Manager", "Back up and restore Controller Paks and their notes", MENU_MODE_CONTROLLER_PAKFS },
    { "Time", "Set the real-time clock", MENU_MODE_RTC },
    { "Menu Information", "Version, credits and licenses", MENU_MODE_CREDITS },
    { "Flashcart Information", "SummerCart64 firmware and features", MENU_MODE_FLASHCART },
    { "N64 Information", "Console region, memory and accessories", MENU_MODE_SYSTEM_INFO },
};

#define ITEM_COUNT  ((int) (sizeof(items) / sizeof(items[0])))

static int selected = 0;


static void process (menu_t *menu) {
    if (ui_components_tab_process(menu, TAB_SETTINGS)) {
        return;
    }

    if (menu->actions.go_up && selected > 0) {
        selected--;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.go_down && selected < ITEM_COUNT - 1) {
        selected++;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.enter) {
        menu->next_mode = items[selected].mode;
        sound_play_effect(SFX_ENTER);
    }
}

static void draw (menu_t *menu, surface_t *display) {
    rdpq_attach_clear(display, NULL);

    ui_components_tab_header_draw(TAB_SETTINGS);

    for (int i = 0; i < ITEM_COUNT; i++) {
        int y = SETTINGS_HUB_Y + (i * SETTINGS_HUB_ROW_PITCH);
        bool is_selected = (i == selected);
        if (is_selected) {
            ui_components_box_draw(CAROUSEL_SELECTED_X, y - 16, CAROUSEL_SELECTED_X + 4, y + 4, RGBA32(0xFF, 0xFF, 0xFF, 0xFF));
        }
        rdpq_text_printf(
            &(rdpq_textparms_t) { .style_id = is_selected ? STL_DEFAULT : STL_GRAY },
            FNT_DEFAULT, CAROUSEL_SELECTED_X + 16, y, "%s", items[i].name
        );
    }

    ui_components_text_draw(
        &(rdpq_textparms_t) { .style_id = STL_GRAY, .width = VISIBLE_AREA_X1 - CAROUSEL_SELECTED_X - 16, .wrap = WRAP_WORD },
        FNT_SMALL, CAROUSEL_SELECTED_X + 16, SETTINGS_HUB_Y + (ITEM_COUNT * SETTINGS_HUB_ROW_PITCH) + 8,
        items[selected].description
    );

    ui_components_button_hint_draw(ICON_A, BUTTON_HINTS_X, LIBRARY_BUTTONS_Y, "Open");

    rdpq_detach_show();
}

void view_settings_hub_init (menu_t *menu) {
}

void view_settings_hub_display (menu_t *menu, surface_t *display) {
    process(menu);
    draw(menu, display);
}
