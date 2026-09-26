/**
 * @file tab_header.c
 * @brief Top-level tabs (Library, Favorites, History, Settings): header strip and L/R switching
 * @ingroup ui_components
 */

#include "../ui_components.h"
#include "../fonts.h"
#include "../sound.h"
#include "constants.h"

static const struct {
    const char *name;
    menu_mode_t mode;
} tabs[TAB_COUNT] = {
    [TAB_LIBRARY] = { "Library", MENU_MODE_BROWSER },
    [TAB_FAVORITES] = { "Favorites", MENU_MODE_FAVORITE },
    [TAB_HISTORY] = { "History", MENU_MODE_HISTORY },
    [TAB_SETTINGS] = { "Settings", MENU_MODE_SETTINGS_HUB },
};


/**
 * @brief Draw the tab names (current one white, the rest gray) and the L / R icons.
 */
void ui_components_tab_header_draw (menu_tab_t current) {
    int x = CAROUSEL_SELECTED_X;
    for (int i = 0; i < TAB_COUNT; i++) {
        rdpq_textmetrics_t metrics = rdpq_text_printf(
            &(rdpq_textparms_t) { .style_id = (i == current) ? STL_DEFAULT : STL_GRAY },
            FNT_DEFAULT, x, LIBRARY_HEADER_Y, "%s", tabs[i].name
        );
        x += (int) (metrics.advance_x) + TAB_HEADER_GAP;
    }

    int r_x = VISIBLE_AREA_X1 - ui_components_icon_width(ICON_R);
    ui_components_icon_draw(ICON_R, r_x, LIBRARY_HEADER_Y - 15);
    ui_components_icon_draw(ICON_L, r_x - 6 - ui_components_icon_width(ICON_L), LIBRARY_HEADER_Y - 15);
}

/**
 * @brief Handle L / R (previous / next tab) and Start (Settings tab).
 *
 * @return true if a tab switch was requested (menu->next_mode is set).
 */
bool ui_components_tab_process (menu_t *menu, menu_tab_t current) {
    menu_tab_t next = current;

    if (menu->actions.tab_prev) {
        next = (current + TAB_COUNT - 1) % TAB_COUNT;
    } else if (menu->actions.tab_next) {
        next = (current + 1) % TAB_COUNT;
    } else if (menu->actions.settings) {
        next = TAB_SETTINGS;
    }

    if (next == current) {
        return false;
    }

    menu->next_mode = tabs[next].mode;
    sound_play_effect(SFX_CURSOR);
    return true;
}
