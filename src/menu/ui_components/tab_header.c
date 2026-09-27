/**
 * @file tab_header.c
 * @brief Top-level tabs (Library, Favorites, History, Settings): header strip and L/R switching
 * @ingroup ui_components
 */

#include <string.h>

#include "../ui_components.h"
#include "../fonts.h"
#include "../sound.h"
#include "constants.h"

static const struct {
    const char *name;
    menu_mode_t mode;
} tabs[TAB_COUNT] = {
    [TAB_LIBRARY] = { "Library", MENU_MODE_BROWSER },
    [TAB_FAVORITES] = { "Faves", MENU_MODE_FAVORITE },
    [TAB_HISTORY] = { "History", MENU_MODE_HISTORY },
    [TAB_SETTINGS] = { "Settings", MENU_MODE_SETTINGS_HUB },
};


// Tab name ink extents (from the pen position), measured once, and the gap between names.
static struct {
    bool measured;
    int ink_x0[TAB_COUNT];
    int ink_width[TAB_COUNT];
    int gap;
    int total;          // ink width from the first name's left edge to the last one's right edge
} layout;


static void measure (void) {
    layout.total = 0;
    for (int i = 0; i < TAB_COUNT; i++) {
        int nbytes = strlen(tabs[i].name);
        rdpq_paragraph_t *paragraph = rdpq_paragraph_build(&(rdpq_textparms_t) { 0 }, TITLE_FONT, tabs[i].name, &nbytes);
        layout.ink_x0[i] = (int) paragraph->bbox.x0;
        layout.ink_width[i] = (int) (paragraph->bbox.x1 - paragraph->bbox.x0);
        rdpq_paragraph_free(paragraph);
        layout.total += layout.ink_width[i];
    }
    // An even total centres on whole pixels; widening every gap by 1px keeps the gaps equal.
    layout.gap = TAB_HEADER_GAP;
    if ((layout.total + (TAB_COUNT - 1) * layout.gap) % 2 != 0) {
        layout.gap++;
    }
    layout.total += (TAB_COUNT - 1) * layout.gap;
    layout.measured = true;
}

/**
 * @brief Draw the tab names (current one white, the rest gray), centred on the screen, with the
 *        L and R pills at either end joined by a bar underneath. Symmetric about DISPLAY_CENTER_X.
 */
void ui_components_tab_header_draw (menu_tab_t current) {
    if (!layout.measured) {
        measure();
    }

    int left = DISPLAY_CENTER_X - (layout.total / 2);
    int x = left;
    for (int i = 0; i < TAB_COUNT; i++) {
        rdpq_text_printf(
            &(rdpq_textparms_t) { .style_id = (i == current) ? STL_DEFAULT : STL_GRAY },
            TITLE_FONT, x - layout.ink_x0[i], LIBRARY_HEADER_Y, "%s", tabs[i].name
        );
        x += layout.ink_width[i] + layout.gap;
    }

    // Bar ends mirror each other about the centre: [left end][bar][right end].
    int half = (layout.total / 2) + TAB_BAR_TEXT_GAP + TAB_BAR_PILL_WIDTH;
    int end_width = ui_components_icon_width(ICON_L);
    int bar_x0 = DISPLAY_CENTER_X - half + end_width;
    int bar_x1 = DISPLAY_CENTER_X + half - end_width;
    int pill_bottom = TAB_BAR_PILL_Y + TAB_BAR_PILL_HEIGHT;
    ui_components_box_draw(bar_x0, pill_bottom - TAB_BAR_HEIGHT, bar_x1, pill_bottom, TAB_BAR_COLOR);
    ui_components_icon_draw(ICON_L, DISPLAY_CENTER_X - half, TAB_BAR_PILL_Y);
    ui_components_icon_draw(ICON_R, bar_x1, TAB_BAR_PILL_Y);
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
