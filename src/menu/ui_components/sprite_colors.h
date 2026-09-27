/**
 * @file sprite_colors.h
 * @brief Fixed colours of the button icons and cartridge art
 * @ingroup ui_components
 *
 * These don't follow the UI palette (palette.h): the button icons and cartridges look the same
 * whichever palette is chosen. Some values currently equal palette colours; they are kept apart
 * on purpose.
 *
 * The sprites are drawn by scripts/make_icons.py and scripts/make_cartridge.py, which read this
 * file (scripts/sprite_colors.py); rerun them after a change. The code draws a few things that
 * have to match the sprites (the tab bar between the L / R icons, folders next to cartridges,
 * the missing-label area) with these colours too.
 * Keep each definition on one line as `#define SPRITE_NAME RGBA32(r, g, b, a)`.
 */

#ifndef SPRITE_COLORS_H__
#define SPRITE_COLORS_H__

#include <libdragon.h>

/* Button icons */
#define SPRITE_BUTTON_A             RGBA32(0x8F, 0xD6, 0x94, 0xFF)  /**< A */
#define SPRITE_BUTTON_B             RGBA32(0xF2, 0x9A, 0x9A, 0xFF)  /**< B */
#define SPRITE_BUTTON_C             RGBA32(0xF5, 0xD7, 0x6E, 0xFF)  /**< C-buttons */
#define SPRITE_BUTTON_GRAY          RGBA32(0xC8, 0xC8, 0xC8, 0xFF)  /**< L, R, Z, and the tab bar joining L and R */
#define SPRITE_BUTTON_GLYPH         RGBA32(0x00, 0x00, 0x00, 0xFF)  /**< Letters and arrows on the buttons */

/* Cartridges (and folders, drawn next to them) */
#define SPRITE_CARTRIDGE_BODY       RGBA32(0xC8, 0xC8, 0xC8, 0xFF)  /**< Body and highlights; folders */
#define SPRITE_CARTRIDGE_DETAIL     RGBA32(0x80, 0x80, 0x80, 0xFF)  /**< Seams, edge, label recess; missing label */
#define SPRITE_OUTLINE_SELECTED     RGBA32(0xFF, 0xFF, 0xFF, 0xFF)  /**< Selected cartridge / folder */
#define SPRITE_OUTLINE_FAVORITE     RGBA32(0xF2, 0xC2, 0x30, 0xFF)  /**< Selected favorite */
#define SPRITE_OUTLINE_HIDDEN       RGBA32(0xF2, 0x9A, 0x9A, 0xFF)  /**< Selected hidden game */

#endif /* SPRITE_COLORS_H__ */
