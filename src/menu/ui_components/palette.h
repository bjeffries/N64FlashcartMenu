/**
 * @file palette.h
 * @brief The UI's colour palette: every colour the menu uses, in one place
 * @ingroup ui_components
 *
 * Screens don't use these directly: constants.h (and fonts.c) give each UI role a colour from
 * here (text, secondary text, dialogs, selection marker, ...), so changing a value below
 * changes it everywhere it is used.
 *
 * Not covered: the button icons and cartridge art, which keep their own colours whatever the
 * palette (sprite_colors.h), and the pre-rendered boot / loading animation (boot_animation/).
 */

#ifndef PALETTE_H__
#define PALETTE_H__

#include <libdragon.h>

/* Black and white */
#define PALETTE_BLACK           RGBA32(0x00, 0x00, 0x00, 0xFF)  /**< Background, text on light badges */
#define PALETTE_WHITE           RGBA32(0xFF, 0xFF, 0xFF, 0xFF)  /**< Text, selection markers, edit underlines */

/* Grays, dark to light: the only four gray tones in the UI */
#define PALETTE_GRAY_1          RGBA32(0x1E, 0x1E, 0x1E, 0xFF)  /**< Dialog background, read-only rows, info badges */
#define PALETTE_GRAY_2          RGBA32(0x40, 0x40, 0x40, 0xFF)  /**< Keyboard keys, bars, unlit page dots and player marks */
#define PALETTE_GRAY_3          RGBA32(0x80, 0x80, 0x80, 0xFF)  /**< Secondary text, active keyboard key */
#define PALETTE_GRAY_4          RGBA32(0xC8, 0xC8, 0xC8, 0xFF)  /**< Lightest gray (not used by a UI role yet) */

/* Text colours of the upstream file list (file types); not used by the carousel screens */
#define PALETTE_LEGACY_GREEN    RGBA32(0x70, 0xFF, 0x70, 0xFF)
#define PALETTE_LEGACY_BLUE     RGBA32(0x70, 0xBC, 0xFF, 0xFF)
#define PALETTE_LEGACY_YELLOW   RGBA32(0xFF, 0xFF, 0x70, 0xFF)
#define PALETTE_LEGACY_ORANGE   RGBA32(0xFF, 0x99, 0x00, 0xFF)
#define PALETTE_LEGACY_RED      RGBA32(0xFF, 0x40, 0x40, 0xFF)

/** @brief A palette colour with a different alpha (e.g. a fade to black). */
#define PALETTE_WITH_ALPHA(color, alpha)    ((color_t) { .r = (color).r, .g = (color).g, .b = (color).b, .a = (alpha) })

#endif /* PALETTE_H__ */
