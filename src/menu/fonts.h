/**
 * @file fonts.h
 * @brief Menu fonts
 * @ingroup menu 
 */

#ifndef FONTS_H__
#define FONTS_H__

#include <stdint.h>

/**
 * @brief Font type enumeration.
 * 
 * This enumeration defines the different types of fonts that can be used
 * in the menu system.
 */
typedef enum {
    FNT_DEFAULT = 1, /**< Default font (sizes per font: MENU_FONT in the Makefile) */
    FNT_TITLE,       /**< Large title font */
    FNT_SMALL,       /**< Small caption font */
    FNT_BAR,         /**< Top and bottom bars: tabs, screen titles, button hints */
    FNT_LAST = FNT_BAR,
} menu_font_type_t;

/**
 * @brief Font style enumeration.
 * 
 * This enumeration defines the different styles of fonts that can be used
 * in the menu system.
 */
typedef enum {
    STL_DEFAULT = 0, /**< Default font style */
    STL_GREEN,       /**< Green font style */
    STL_BLUE,        /**< Blue font style */
    STL_YELLOW,      /**< Yellow font style */
    STL_ORANGE,      /**< Orange font style */
    STL_RED,         /**< Red font style */
    STL_GRAY,        /**< Gray font style */
    STL_BLACK,       /**< Black font style (text on light badges) */
    STL_FADE,        /**< Between background and text colour, set with fonts_set_fade_level() */
    STL_SHADOW,      /**< Text shadow: dimmer than text, lighter than the background */
} menu_font_style_t;

/**
 * @brief Initialize fonts.
 * 
 * This function initializes the fonts used in the menu system. It can load
 * custom fonts from the specified path.
 * 
 * @param custom_font_path Path to the custom font file.
 */
void fonts_init(char *custom_font_path);

/**
 * @brief Recolour the text styles of every font after switching palettes.
 */
void fonts_apply_palette(void);

/**
 * @brief Height of a font's capital letters in pixels (measured when the fonts load).
 */
int fonts_cap_height(menu_font_type_t id);

/**
 * @brief A font's ascent (line height above the baseline) in pixels; at least its cap height.
 */
int fonts_ascent(menu_font_type_t id);

/**
 * @brief Set the STL_FADE colour of a font: 0 is the background colour, 255 the text colour.
 */
void fonts_set_fade_level(uint8_t font_id, uint8_t level);

#endif /* FONTS_H__ */
