/**
 * @file palette.h
 * @brief The UI's colour palettes: five colours each, chosen in Menu Settings
 * @ingroup ui_components
 *
 * Every palette has a background, three tones (dark to light) and a highlight, which is also
 * the main text colour. Screens don't use these directly: constants.h (and fonts.c) give each UI
 * role a palette colour (text, secondary text, dialogs, selection marker, ...). The colours are
 * read at draw time, so switching palettes recolours the whole UI at once.
 *
 * Not covered: the button icons and cartridge art, which keep their own colours whatever the
 * palette (sprite_colors.h), and the boot / loading animation, which always plays on black.
 * The palettes themselves are defined in palette.c.
 */

#ifndef PALETTE_H__
#define PALETTE_H__

#include <libdragon.h>

/** @brief The palettes, in the order Menu Settings lists them: Dusk (the default), then A-Z. */
typedef enum {
    UI_PALETTE_DUSK,
    UI_PALETTE_DAWN,
    UI_PALETTE_GALAXY,
    UI_PALETTE_MONOCHROME,
    UI_PALETTE_COUNT,
    UI_PALETTE_DEFAULT = UI_PALETTE_DUSK,       /**< Before settings load, and for unknown keys */
} ui_palette_id_t;

/** @brief One palette: five colours. */
typedef struct {
    const char *name;       /**< Shown in Menu Settings */
    const char *key;        /**< Saved in config.ini */
    color_t background;     /**< Screen background */
    color_t tone_1;         /**< Darkest tone: dialog background, read-only rows, info badges */
    color_t tone_2;         /**< Keyboard keys, bars, unlit page dots and player marks */
    color_t tone_3;         /**< Lightest tone: secondary text, active keyboard key */
    color_t highlight;      /**< Main text, selection markers, lit dots, edit underlines */
} ui_palette_t;

/** @brief The palette in use. */
extern const ui_palette_t *ui_palette;

/** @brief Switch palettes (call fonts_apply_palette() afterwards to recolour text). */
void ui_palette_set(ui_palette_id_t id);
/** @brief The palette in use. */
ui_palette_id_t ui_palette_get(void);
/** @brief A palette's details. */
const ui_palette_t *ui_palette_info(ui_palette_id_t id);
/** @brief The palette saved under a config.ini key ("galaxy", ...); the default if unknown. */
ui_palette_id_t ui_palette_from_key(const char *key);

/* The current palette's colours */
#define PALETTE_BACKGROUND      (ui_palette->background)
#define PALETTE_TONE_1          (ui_palette->tone_1)
#define PALETTE_TONE_2          (ui_palette->tone_2)
#define PALETTE_TONE_3          (ui_palette->tone_3)
#define PALETTE_HIGHLIGHT       (ui_palette->highlight)

/* Text colours of the upstream file list (file types); not used by the carousel screens */
#define PALETTE_LEGACY_GREEN    RGBA32(0x70, 0xFF, 0x70, 0xFF)
#define PALETTE_LEGACY_BLUE     RGBA32(0x70, 0xBC, 0xFF, 0xFF)
#define PALETTE_LEGACY_YELLOW   RGBA32(0xFF, 0xFF, 0x70, 0xFF)
#define PALETTE_LEGACY_ORANGE   RGBA32(0xFF, 0x99, 0x00, 0xFF)
#define PALETTE_LEGACY_RED      RGBA32(0xFF, 0x40, 0x40, 0xFF)

/** @brief Mix two colours: amount 0 is a, 255 is b. */
static inline color_t palette_mix (color_t a, color_t b, int amount) {
    return RGBA32(
        a.r + (((b.r - a.r) * amount) / 255),
        a.g + (((b.g - a.g) * amount) / 255),
        a.b + (((b.b - a.b) * amount) / 255),
        0xFF
    );
}

/** @brief A colour with a different alpha (e.g. a fade to black). */
#define PALETTE_WITH_ALPHA(color, alpha)    ((color_t) { .r = (color).r, .g = (color).g, .b = (color).b, .a = (alpha) })

#endif /* PALETTE_H__ */
