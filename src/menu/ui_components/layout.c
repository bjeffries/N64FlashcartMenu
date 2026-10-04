/**
 * @file layout.c
 * @brief Where the Library's carousel, title and info panel go (Menu Settings > Carousel Position)
 * @ingroup ui_components
 *
 * Carousel at the top: tabs, carousel, then the info panel (title, rows and screenshot).
 * At the bottom (default): tabs, the info panel where the carousel was, then the carousel, its captions
 * ending a little above where the screenshot's shadow ended in the top layout.
 */

#include <libdragon.h>

#include "../fonts.h"
#include "../ui_components.h"
#include "constants.h"

static bool carousel_bottom = false;


void ui_components_layout_set (bool carousel_at_bottom) {
    carousel_bottom = carousel_at_bottom;
}

/** @brief Baseline of the title with the carousel at the top. */
static int top_title_y (void) {
    return CAROUSEL_TOP_TILE_Y + CAROUSEL_CAPTION_OFFSET + 22 + CAROUSEL_TITLE_CAPS;
}

/** @brief Top of the screenshot for info rows starting at info_y. */
static int screenshot_top (int info_y) {
    return info_y - fonts_cap_height(GAME_INFO_FONT) - 8;
}

/** @brief Bottom of the screenshot's frame and shadow for info rows starting at info_y. */
static int screenshot_shadow_bottom (int info_y) {
    return screenshot_top(info_y) + GAME_INFO_SCREENSHOT_HEIGHT + GAME_INFO_SCREENSHOT_OUTLINE + GAME_INFO_SCREENSHOT_SHADOW;
}

int ui_components_layout_title_y (void) {
    // At the bottom: the title's capitals start where the selected cartridge's outline did.
    return carousel_bottom ? (CAROUSEL_TOP_TILE_Y + CAROUSEL_OUTLINE_TOP + CAROUSEL_TITLE_CAPS) : (top_title_y() - CAROUSEL_TOP_LAYOUT_RAISE);
}

int ui_components_layout_info_y (void) {
    // With hints off, the info panel moves down into the freed space only when it's at the bottom.
    int shift = (!carousel_bottom && !ui_components_button_hints_enabled()) ? GAME_INFO_NO_HINTS_SHIFT : 0;
    return ui_components_layout_title_y() + 30 + shift;
}

int ui_components_layout_carousel_y (void) {
    if (!carousel_bottom) {
        return CAROUSEL_TOP_TILE_Y - CAROUSEL_TOP_LAYOUT_RAISE;
    }
    // Captions end CAROUSEL_BOTTOM_RAISE above where the screenshot's shadow ends in the top layout
    // (lower with hints off), clear of the button hints.
    int top_info_y = top_title_y() + 30 + (ui_components_button_hints_enabled() ? 0 : GAME_INFO_NO_HINTS_SHIFT);
    return screenshot_shadow_bottom(top_info_y) - CAROUSEL_CAPTION_OFFSET - CAROUSEL_BOTTOM_RAISE;
}

int ui_components_layout_info_bottom (void) {
    if (carousel_bottom) {
        return screenshot_top(ui_components_layout_info_y()) + GAME_INFO_SCREENSHOT_HEIGHT;
    }
    return ui_components_button_hints_enabled() ? (LIBRARY_BUTTONS_Y - 34) : (VISIBLE_AREA_Y1 - 12);
}

int ui_components_layout_loading_y (void) {
    int info_y = ui_components_layout_info_y();
    // Centred between the top of the first info row and the top of the "Loading" text (on the
    // button hints' baseline), or with the carousel at the bottom, the bottom of the info panel.
    int bottom = carousel_bottom ? screenshot_shadow_bottom(info_y) : (LIBRARY_BUTTONS_Y - 14);
    return ((info_y - 14) + bottom) / 2;
}
