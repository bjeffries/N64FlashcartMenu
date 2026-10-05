/**
 * @file screensaver.h
 * @brief Screensaver: the spinning N64 logo bouncing around the screen after a while without input
 * @ingroup menu
 */

#ifndef SCREENSAVER_H__
#define SCREENSAVER_H__

#include <stdbool.h>

#include <libdragon.h>

#include "menu_state.h"

/** @brief Seconds without input before the screensaver starts (Menu Settings > Screensaver); 0 = never. */
void screensaver_set_timeout(int seconds);

/** @brief Screensaver styles (Menu Settings > Screensaver Style), in the picker's order. */
typedef enum {
    SCREENSAVER_RANDOM,     /**< A different one each time */
    SCREENSAVER_BOUNCE,
    SCREENSAVER_TRAILS,
    SCREENSAVER_GRID,
    SCREENSAVER_STARFIELD,
    SCREENSAVER_ORBIT,
    SCREENSAVER_STYLE_COUNT,
} screensaver_style_setting_t;

/** @brief The style's config.ini value ("random", "bounce", ...). */
const char *screensaver_style_key(screensaver_style_setting_t style);
/** @brief The style for a config.ini value (Random if it isn't one). */
screensaver_style_setting_t screensaver_style_from_key(const char *key);
/** @brief Use this style from the next time the screensaver starts. */
void screensaver_set_style(screensaver_style_setting_t style);

/**
 * @brief Start, stop or keep up the screensaver, after actions_update() each frame. While it shows
 *        and for a moment after it's woken, the menu's actions are cleared so it ignores the input.
 * @return Whether the screensaver is showing: draw it with screensaver_draw() instead of the screen.
 */
bool screensaver_update(menu_t *menu);

/** @brief Move the logo on and draw the screensaver. */
void screensaver_draw(surface_t *display);

#endif
