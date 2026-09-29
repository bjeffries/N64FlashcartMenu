/**
 * @file icons.c
 * @brief Controller button icons and button hints ("(A) Play Cartridge")
 * @ingroup ui_components
 */

#include <string.h>

#include "../ui_components.h"
#include "../fonts.h"
#include "constants.h"

#define HINT_ICON_GAP   (6)
#define HINT_PAIR_GAP       (4)     // space either side of the square between a pair of icons
#define HINT_PAIR_SQUARE    (3)     // the square, like the one between accessories in the game info

static const char *icon_paths[ICON_COUNT] = {
    [ICON_A] = "rom:/button_a.sprite",
    [ICON_B] = "rom:/button_b.sprite",
    [ICON_C_RIGHT] = "rom:/button_c_right.sprite",
    [ICON_C_UP] = "rom:/button_c_up.sprite",
    [ICON_C_LEFT] = "rom:/button_c_left.sprite",
    [ICON_C_DOWN] = "rom:/button_c_down.sprite",
    [ICON_Z] = "rom:/button_z.sprite",
    [ICON_START] = "rom:/button_start.sprite",
    [ICON_L] = "rom:/tab_l.sprite",     // tab bar ends: pill + curve into the bar
    [ICON_R] = "rom:/tab_r.sprite",
};

static sprite_t *icons[ICON_COUNT];


static sprite_t *icon_get (ui_icon_t icon) {
    if (!icons[icon]) {
        icons[icon] = sprite_load(icon_paths[icon]);
    }
    return icons[icon];
}

/**
 * @brief Draw a button icon with its top-left corner at (x, y).
 */
void ui_components_icon_draw (ui_icon_t icon, int x, int y) {
    rdpq_mode_push();
        rdpq_set_mode_copy(true);
        rdpq_sprite_blit(icon_get(icon), x, y, NULL);
    rdpq_mode_pop();
}

/**
 * @brief Width of a button icon (pixels).
 */
int ui_components_icon_width (ui_icon_t icon) {
    return icon_get(icon)->width;
}

/**
 * @brief Draw a button icon followed by a label, vertically centred on the label's capitals.
 *
 * @return Horizontal space used, so hints can be laid out one after another.
 */
static float text_advance (const char *text, bool ink_only) {
    int nbytes = strlen(text);
    rdpq_paragraph_t *paragraph = rdpq_paragraph_build(&(rdpq_textparms_t) { 0 }, TITLE_FONT, text, &nbytes);
    float width = ink_only ? paragraph->bbox.x1 : paragraph->advance_x;
    rdpq_paragraph_free(paragraph);
    return width;
}

/** @brief Extra width of an "(A) ▪ (Start)" pair over a single icon: the square and the second icon. */
static int pair_extra_width (ui_icon_t second) {
    return HINT_PAIR_GAP + HINT_PAIR_SQUARE + HINT_PAIR_GAP + icon_get(second)->width;
}

/**
 * @brief Draw a row of button hints centred at the bottom of the screen.
 *
 * Centred on what is drawn: from the first icon's left edge to the last label's last pixel.
 */
static bool hints_enabled = true;

void ui_components_button_hints_enable (bool enabled) {
    hints_enabled = enabled;
}

void ui_components_button_hints_draw (const button_hint_t *hints, int count) {
    if (count <= 0 || !hints_enabled) {
        return;
    }
    int width = 0;
    for (int i = 0; i < count; i++) {
        bool last = (i == count - 1);
        width += icon_get(hints[i].icon)->width + HINT_ICON_GAP + (int) (text_advance(hints[i].text, last) + 0.5f);
        if (hints[i].stacked) {
            width += pair_extra_width(hints[i].behind);
        }
        if (!last) {
            width += LIBRARY_HINT_GAP;
        }
    }
    int x = DISPLAY_CENTER_X - (width / 2);
    for (int i = 0; i < count; i++) {
        if (hints[i].stacked) {
            x += ui_components_button_hint_draw_stacked(hints[i].icon, hints[i].behind, x, LIBRARY_BUTTONS_Y, hints[i].text) + LIBRARY_HINT_GAP;
        } else {
            x += ui_components_button_hint_draw(hints[i].icon, x, LIBRARY_BUTTONS_Y, hints[i].text) + LIBRARY_HINT_GAP;
        }
    }
}

/**
 * @brief A hint for two buttons that do the same thing: "(A) ▪ (Start) Label", both icons level
 *        with the other hints, separated by a small square as accessories are in the game info.
 */
int ui_components_button_hint_draw_stacked (ui_icon_t icon, ui_icon_t behind, int x, int baseline, const char *text) {
    sprite_t *sprite = icon_get(icon);
    int y = baseline - (fonts_cap_height(TITLE_FONT) / 2) - (sprite->height / 2);
    ui_components_icon_draw(icon, x, y);

    int square_x = x + sprite->width + HINT_PAIR_GAP;
    int square_y = y + (sprite->height / 2) - (HINT_PAIR_SQUARE / 2);
    ui_components_box_draw(square_x, square_y, square_x + HINT_PAIR_SQUARE, square_y + HINT_PAIR_SQUARE, TEXT_SECONDARY_COLOR);
    int second_x = square_x + HINT_PAIR_SQUARE + HINT_PAIR_GAP;
    ui_components_icon_draw(behind, second_x, y);

    int text_x = second_x + icon_get(behind)->width + HINT_ICON_GAP;
    rdpq_textmetrics_t metrics = rdpq_text_printf(
        &(rdpq_textparms_t) { .style_id = STL_DEFAULT },
        TITLE_FONT, text_x, baseline, "%s", text
    );
    return (text_x - x) + (int) (metrics.advance_x);
}

int ui_components_button_hint_draw (ui_icon_t icon, int x, int baseline, const char *text) {
    sprite_t *sprite = icon_get(icon);
    // Centred on the label's capitals.
    ui_components_icon_draw(icon, x, baseline - (fonts_cap_height(TITLE_FONT) / 2) - (sprite->height / 2));

    int text_x = x + sprite->width + HINT_ICON_GAP;
    rdpq_textmetrics_t metrics = rdpq_text_printf(
        &(rdpq_textparms_t) { .style_id = STL_DEFAULT },
        TITLE_FONT, text_x, baseline, "%s", text
    );
    return (text_x - x) + (int) (metrics.advance_x);
}
