/**
 * @file palette.h
 * @brief The UI's colour palette: every colour the menu uses, in one place
 * @ingroup ui_components
 *
 * Screens don't use these directly: constants.h (and fonts.c) give each UI role a colour from
 * here (TAB_BAR_COLOR, CAROUSEL_FOLDER_COLOR, the text styles, ...), so changing a value below
 * changes it everywhere it is used.
 *
 * The sprite scripts (scripts/make_icons.py, scripts/make_cartridge.py) read this file too
 * (scripts/palette.py), so the button icons and cartridges follow it; rerun them after a change.
 * Keep each definition on one line as `#define PALETTE_NAME RGBA32(r, g, b, a)` for that.
 *
 * Not covered: the boot / loading animation, which is pre-rendered (boot_animation/).
 */

#ifndef PALETTE_H__
#define PALETTE_H__

#include <libdragon.h>

/* Black and white */
#define PALETTE_BLACK           RGBA32(0x00, 0x00, 0x00, 0xFF)  /**< Background, text on light badges */
#define PALETTE_WHITE           RGBA32(0xFF, 0xFF, 0xFF, 0xFF)  /**< Text, selection outline and markers */

/* Grays, dark to light */
#define PALETTE_GRAY_1          RGBA32(0x11, 0x11, 0x11, 0xFF)  /**< Dialog background */
#define PALETTE_GRAY_2          RGBA32(0x1E, 0x1E, 0x1E, 0xFF)  /**< Read-only rows, info badges */
#define PALETTE_GRAY_3          RGBA32(0x33, 0x33, 0x33, 0xFF)  /**< Keyboard keys, bars */
#define PALETTE_GRAY_4          RGBA32(0x3F, 0x3F, 0x3F, 0xFF)  /**< Upstream screens: scrollbar, tabs */
#define PALETTE_GRAY_5          RGBA32(0x55, 0x55, 0x55, 0xFF)  /**< Unlit page dots and player marks */
#define PALETTE_GRAY_6          RGBA32(0x5F, 0x5F, 0x5F, 0xFF)  /**< Upstream screens: scrollbar, tabs */
#define PALETTE_GRAY_7          RGBA32(0x6A, 0x6A, 0x6A, 0xFF)  /**< Missing label, cartridge recess */
#define PALETTE_GRAY_8          RGBA32(0x70, 0x70, 0x70, 0xFF)  /**< Active keyboard key (SHIFT) */
#define PALETTE_GRAY_9          RGBA32(0x80, 0x80, 0x80, 0xFF)  /**< Secondary text */
#define PALETTE_GRAY_10         RGBA32(0x8A, 0x8A, 0x8A, 0xFF)  /**< Cartridge seams */
#define PALETTE_GRAY_11         RGBA32(0x9A, 0x9A, 0x9A, 0xFF)  /**< Cartridge edge */
#define PALETTE_GRAY_12         RGBA32(0xC4, 0xC4, 0xC4, 0xFF)  /**< Cartridge body, folders */
#define PALETTE_GRAY_13         RGBA32(0xC8, 0xC8, 0xC8, 0xFF)  /**< Tab bar, L / R / Z buttons */
#define PALETTE_GRAY_14         RGBA32(0xD8, 0xD8, 0xD8, 0xFF)  /**< Cartridge highlights */

/* Accents */
#define PALETTE_GREEN           RGBA32(0x8F, 0xD6, 0x94, 0xFF)  /**< A button */
#define PALETTE_RED             RGBA32(0xF2, 0x9A, 0x9A, 0xFF)  /**< B button, hidden games */
#define PALETTE_YELLOW          RGBA32(0xF5, 0xD7, 0x6E, 0xFF)  /**< C buttons */
#define PALETTE_GOLD            RGBA32(0xF2, 0xC2, 0x30, 0xFF)  /**< Favorites */

/* Text colours of the upstream file list (file types); not used by the carousel screens */
#define PALETTE_LEGACY_GREEN    RGBA32(0x70, 0xFF, 0x70, 0xFF)
#define PALETTE_LEGACY_BLUE     RGBA32(0x70, 0xBC, 0xFF, 0xFF)
#define PALETTE_LEGACY_YELLOW   RGBA32(0xFF, 0xFF, 0x70, 0xFF)
#define PALETTE_LEGACY_ORANGE   RGBA32(0xFF, 0x99, 0x00, 0xFF)
#define PALETTE_LEGACY_RED      RGBA32(0xFF, 0x40, 0x40, 0xFF)

/** @brief A palette colour with a different alpha (e.g. a fade to black). */
#define PALETTE_WITH_ALPHA(color, alpha)    ((color_t) { .r = (color).r, .g = (color).g, .b = (color).b, .a = (alpha) })

#endif /* PALETTE_H__ */
