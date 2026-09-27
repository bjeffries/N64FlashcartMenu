/**
 * @file icons.c
 * @brief Controller button icons and button hints ("(A) Play Cartridge")
 * @ingroup ui_components
 */

#include "../ui_components.h"
#include "../fonts.h"
#include "constants.h"

#define HINT_ICON_GAP   (6)

static const char *icon_paths[ICON_COUNT] = {
    [ICON_A] = "rom:/button_a.sprite",
    [ICON_B] = "rom:/button_b.sprite",
    [ICON_C_RIGHT] = "rom:/button_c_right.sprite",
    [ICON_C_UP] = "rom:/button_c_up.sprite",
    [ICON_C_LEFT] = "rom:/button_c_left.sprite",
    [ICON_C_DOWN] = "rom:/button_c_down.sprite",
    [ICON_Z] = "rom:/button_z.sprite",
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
int ui_components_button_hint_draw (ui_icon_t icon, int x, int baseline, const char *text) {
    sprite_t *sprite = icon_get(icon);
    // FNT_DEFAULT capitals are 14px tall, so their centre is 7px above the baseline.
    ui_components_icon_draw(icon, x, baseline - 7 - (sprite->height / 2));

    int text_x = x + sprite->width + HINT_ICON_GAP;
    rdpq_textmetrics_t metrics = rdpq_text_printf(
        &(rdpq_textparms_t) { .style_id = STL_DEFAULT },
        FNT_DEFAULT, text_x, baseline, "%s", text
    );
    return (text_x - x) + (int) (metrics.advance_x);
}
