#include "views.h"
#include "../sound.h"
#include "../ui_components/constants.h"


static void process (menu_t *menu) {
    if (menu->actions.back || menu->actions.enter) {
        sound_play_effect(SFX_EXIT);
        menu->next_mode = menu->error_return_mode;
    }
}

static void draw (menu_t *menu, surface_t *d) {
    rdpq_attach_clear(d, NULL);

    ui_components_messagebox_draw(menu->error_message ? menu->error_message : "Something went wrong");

    ui_components_button_hint_draw(ICON_B, GAME_INFO_VALUE_X, LIBRARY_BUTTONS_Y, "Back");

    rdpq_detach_show();
}

static void deinit (menu_t *menu) {
    menu->error_message = NULL;
    menu->flashcart_err = FLASHCART_OK;
}


void view_error_init (menu_t *menu) {
    if (menu->flashcart_err != FLASHCART_OK) {
        debugf(
            "Flashcart error [%d]: %s\n",
            menu->flashcart_err,
            flashcart_convert_error_message(menu->flashcart_err)
        );
    }
}

void view_error_display (menu_t *menu, surface_t *display) {
    process(menu);

    draw(menu, display);

    if (menu->next_mode != MENU_MODE_ERROR) {
        deinit(menu);
    }
}

void menu_show_error (menu_t *menu, char *error_message) {
    sound_play_effect(SFX_ERROR);

    // Go back to the screen that raised the error, except for screens that would just retry the
    // failing load: those return to the tab the game was launched from.
    switch (menu->mode) {
        case MENU_MODE_LOAD_ROM:
        case MENU_MODE_LOAD_DISK:
        case MENU_MODE_DATEL_CODE_EDITOR:
            menu->error_return_mode = menu->load.return_mode;
            break;
        case MENU_MODE_NONE:
        case MENU_MODE_STARTUP:
        case MENU_MODE_ERROR:
        case MENU_MODE_FAULT:
        case MENU_MODE_BOOT:
            menu->error_return_mode = MENU_MODE_BROWSER;
            break;
        default:
            menu->error_return_mode = menu->mode;
            break;
    }

    menu->next_mode = MENU_MODE_ERROR;
    menu->error_message = error_message;
}
