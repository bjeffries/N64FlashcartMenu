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

/**
 * @brief Start, stop or keep up the screensaver, after actions_update() each frame. While it shows
 *        and for a moment after it's woken, the menu's actions are cleared so it ignores the input.
 * @return Whether the screensaver is showing: draw it with screensaver_draw() instead of the screen.
 */
bool screensaver_update(menu_t *menu);

/** @brief Move the logo on and draw the screensaver. */
void screensaver_draw(surface_t *display);

#endif
