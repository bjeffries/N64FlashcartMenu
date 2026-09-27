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
    FNT_DEFAULT = 1, /**< Default font type (Analogue OS 20px) */
    FNT_TITLE,       /**< Large title font (Analogue OS 40px) */
    FNT_SMALL,       /**< Small caption font (Analogue OS 12px, anti-aliased) */
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
    STL_FADE,        /**< Gray level set with fonts_set_fade_level() (fading text on black) */
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
 * @brief Set the STL_FADE colour of a font to a gray level (0 black - 255 white).
 */
void fonts_set_fade_level(uint8_t font_id, uint8_t level);

#endif /* FONTS_H__ */
