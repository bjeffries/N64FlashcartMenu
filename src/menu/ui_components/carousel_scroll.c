/**
 * @file carousel_scroll.c
 * @brief Left / right scrolling for carousel lists, speeding up while held
 * @ingroup ui_components
 *
 * Holding ←/→ moves CAROUSEL_SPEED_1 tiles a second, then CAROUSEL_SPEED_2 after
 * CAROUSEL_SPEED_STEP_MS and CAROUSEL_SPEED_3 after twice that; letting go drops straight back.
 * While held, the first letter of the file name (what the lists are sorted by; '#' for names that
 * don't start with a letter) shows against the right margin, level with the game title, each time
 * it changes: large, on a half-transparent black rounded box. It stays while the scroll is held and
 * fades out after letting go. A single press neither speeds up nor shows the letter. The cursor sound plays at most every
 * CAROUSEL_SOUND_MIN_MS, so fast scrolling ticks instead of buzzing.
 */

#include <ctype.h>
#include <math.h>
#include <string.h>

#include "../ui_components.h"
#include "../fonts.h"
#include "../sound.h"
#include "constants.h"
#include "utils/utils.h"

// Holds pause for the menu's key-repeat delay (~270ms) after the first press: a gap shorter than
// this is still the same hold.
#define HOLD_GAP_MS     (300)

static struct {
    int direction;          // -1 / 1 while held, 0 when not
    int moves;              // tiles moved in this hold
    uint32_t hold_start_ms;
    uint32_t last_seen_ms;  // last frame the direction was held
    uint32_t next_move_ms;
    uint32_t last_sound_ms;
} scroll;

// Something that fades in when shown, stays while it keeps being updated, then fades out.
typedef struct {
    uint32_t shown_ms;      // when it started showing (fade in)
    uint32_t updated_ms;    // last update (fade out after this)
    bool visible;
} fade_t;

static struct {
    char letter;
    fade_t fade;
} indicator;

// Position counter ("12/87"): shown on every selection change (Menu Settings > Game Counter).
static fade_t position;
static bool position_enabled = false;


static void fade_touch (fade_t *fade, uint32_t now) {
    if (!fade->visible) {
        fade->shown_ms = now;
    }
    fade->visible = true;
    fade->updated_ms = now;
}

/** @brief Current brightness (0-255), or -1 once it has faded out. */
static int fade_level (fade_t *fade) {
    if (!fade->visible) {
        return -1;
    }
    uint32_t now = get_ticks_ms();
    uint32_t since_shown = now - fade->shown_ms;
    uint32_t since_update = now - fade->updated_ms;

    int level = 0xFF;
    if (since_shown < LETTER_INDICATOR_FADE_IN_MS) {
        level = (since_shown * 0xFF) / LETTER_INDICATOR_FADE_IN_MS;
    }
    if (since_update > LETTER_INDICATOR_HOLD_MS) {
        uint32_t fading = since_update - LETTER_INDICATOR_HOLD_MS;
        if (fading >= LETTER_INDICATOR_FADE_OUT_MS) {
            fade->visible = false;
            return -1;
        }
        level = MIN(level, (int) (((LETTER_INDICATOR_FADE_OUT_MS - fading) * 0xFF) / LETTER_INDICATOR_FADE_OUT_MS));
    }
    return level;
}


static const char *base_name (const char *name) {
    const char *slash = strrchr(name, '/');
    return slash ? slash + 1 : name;
}

static char letter_of (entry_t *entry) {
    char c = toupper((unsigned char) base_name(entry->name)[0]);
    return (c >= 'A' && c <= 'Z') ? c : '#';
}

/**
 * @brief Forget any held direction (call when a carousel view opens).
 */
void ui_components_carousel_scroll_reset (void) {
    scroll.direction = 0;
    scroll.moves = 0;
}

static int32_t scroll_step (menu_t *menu, entry_t *list, int32_t count, int32_t selected, bool letter_hint);

/**
 * @brief Handle ←/→ for a carousel list (circular).
 *
 * @param letter_hint Whether to show the first letter while held (lists sorted by file name).
 * @return The new selection.
 */
int32_t ui_components_carousel_scroll (menu_t *menu, entry_t *list, int32_t count, int32_t selected, bool letter_hint) {
    int32_t next = scroll_step(menu, list, count, selected, letter_hint);
    if (next != selected) {
        fade_touch(&position, get_ticks_ms());
    }
    return next;
}

/** @brief Time between tiles this long into a hold. */
static uint32_t move_interval (uint32_t held_ms) {
    int speed = (held_ms < CAROUSEL_SPEED_STEP_MS) ? CAROUSEL_SPEED_1
        : (held_ms < (2 * CAROUSEL_SPEED_STEP_MS)) ? CAROUSEL_SPEED_2
        : CAROUSEL_SPEED_3;
    return 1000 / speed;
}

static int32_t scroll_step (menu_t *menu, entry_t *list, int32_t count, int32_t selected, bool letter_hint) {
    uint32_t now = get_ticks_ms();
    // C-buttons also report a direction (go_fast); in carousels they are action buttons instead.
    bool horizontal = (menu->actions.go_left || menu->actions.go_right) && !menu->actions.go_fast;
    if (!horizontal || count < 2) {
        if (scroll.direction != 0 && (now - scroll.last_seen_ms) > HOLD_GAP_MS) {
            ui_components_carousel_scroll_reset();
        }
        return selected;
    }

    int direction = menu->actions.go_left ? -1 : 1;
    if (direction != scroll.direction) {
        // A new press (or a change of direction): move straight away, then keep going while held.
        scroll.direction = direction;
        scroll.moves = 0;
        scroll.hold_start_ms = now;
        scroll.next_move_ms = now;
    }
    scroll.last_seen_ms = now;

    // Once shown, the letter stays up for as long as the scroll is held (it fades after letting go).
    if (letter_hint && scroll.moves > 0 && indicator.fade.visible) {
        fade_touch(&indicator.fade, now);
    }

    if ((int32_t) (now - scroll.next_move_ms) < 0) {
        return selected;
    }
    uint32_t interval = move_interval(now - scroll.hold_start_ms);
    scroll.next_move_ms += interval;
    if ((int32_t) (now - scroll.next_move_ms) > (int32_t) interval) {
        scroll.next_move_ms = now + interval;   // fell behind (a slow frame): don't catch up in a burst
    }

    int32_t next = (selected + count + direction) % count;
    if (letter_hint && scroll.moves > 0 && letter_of(&list[next]) != letter_of(&list[selected])) {
        indicator.letter = letter_of(&list[next]);
        fade_touch(&indicator.fade, now);
    }
    scroll.moves++;
    if ((now - scroll.last_sound_ms) >= CAROUSEL_SOUND_MIN_MS) {
        scroll.last_sound_ms = now;
        sound_play_effect(SFX_CURSOR);
    }
    return next;
}

/**
 * @brief Draw the letter being paged to in the top-left corner, fading in and out.
 */
void ui_components_letter_indicator_draw (void) {
    int level = fade_level(&indicator.fade);
    if (level < 0) {
        return;
    }

    int size = LETTER_INDICATOR_BOX_SIZE;
    int x0 = LETTER_INDICATOR_BOX_RIGHT - size;
    int y0 = LETTER_INDICATOR_BOX_TOP;
    int r = LETTER_INDICATOR_BOX_RADIUS;

    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
        rdpq_set_prim_color(RGBA32(0, 0, 0, (LETTER_INDICATOR_BOX_ALPHA * level) / 0xFF));
        // Rounded corners: the top and bottom r rows are inset along a quarter circle; each row
        // is drawn once so the transparency stays even.
        for (int row = 0; row < r; row++) {
            float dy = r - row - 0.5f;
            int inset = (int) (r - sqrtf((float) (r * r) - (dy * dy)) + 0.5f);
            rdpq_fill_rectangle(x0 + inset, y0 + row, x0 + size - inset, y0 + row + 1);
            rdpq_fill_rectangle(x0 + inset, y0 + size - row - 1, x0 + size - inset, y0 + size - row);
        }
        rdpq_fill_rectangle(x0, y0 + r, x0 + size, y0 + size - r);
    rdpq_mode_pop();

    // The title text's colour, fading with the box.
    color_t colour = TEXT_COLOR;
    colour.a = (uint8_t) level;
    rdpq_font_style((rdpq_font_t *) rdpq_text_get_font(FNT_LETTER), STL_FADE, &((rdpq_fontstyle_t) { .color = colour }));

    // Centred on the box: across by the letter's ink, down by its capitals.
    char text[2] = { indicator.letter, '\0' };
    int nbytes = 1;
    rdpq_paragraph_t *layout = rdpq_paragraph_build(&(rdpq_textparms_t) { .style_id = STL_FADE }, FNT_LETTER, text, &nbytes);
    float ink_centre = (layout->bbox.x0 + layout->bbox.x1) / 2.0f;
    rdpq_paragraph_render(layout,
        (int) (x0 + (size / 2) - ink_centre + 0.5f),
        y0 + (size / 2) + (fonts_cap_height(FNT_LETTER) / 2));
    rdpq_paragraph_free(layout);
}

/**
 * @brief Draw the position counter in the top-right corner, mirroring the letter indicator: a
 *        vertical fraction (index over total) in body text, narrow enough to sit clear of the
 *        tab bar's R end. It fades in on every selection change and out after a pause.
 *
 * @param index 1-based position of the selection, or 0 to draw nothing (e.g. a folder).
 * @param total Number of entries counted.
 */
void ui_components_position_indicator_enable (bool enabled) {
    position_enabled = enabled;
}

void ui_components_position_indicator_draw (int index, int total) {
    int level = fade_level(&position);
    if (!position_enabled || level < 0 || index <= 0 || total <= 0) {
        return;
    }
    char numerator[12], denominator[12];
    snprintf(numerator, sizeof(numerator), "%d", index);
    snprintf(denominator, sizeof(denominator), "%d", total);

    // Both numbers centred on one column whose right edge mirrors the letter indicator's left edge.
    int width = 0;
    int ink_x0[2], ink_x1[2];
    const char *lines[2] = { numerator, denominator };
    for (int i = 0; i < 2; i++) {
        int nbytes = strlen(lines[i]);
        rdpq_paragraph_t *layout = rdpq_paragraph_build(&(rdpq_textparms_t) { 0 }, BODY_FONT, lines[i], &nbytes);
        ink_x0[i] = (int) layout->bbox.x0;
        ink_x1[i] = (int) layout->bbox.x1;
        rdpq_paragraph_free(layout);
        width = MAX(width, ink_x1[i] - ink_x0[i]);
    }
    // The bar reaches past the wider number on both sides (and is never shorter than a minimum).
    int bar_width = MAX(POSITION_FRACTION_BAR_MIN, width + (2 * POSITION_FRACTION_BAR_OVERHANG));
    int right = POSITION_INDICATOR_RIGHT;
    int centre = right - (bar_width / 2);

    // Top of the numerator level with the top of the letter indicator's capitals.
    int cap = fonts_cap_height(BODY_FONT);
    int top = POSITION_INDICATOR_TOP;
    int baselines[2] = { top + cap, top + cap + POSITION_FRACTION_GAP + POSITION_FRACTION_BAR + POSITION_FRACTION_GAP + cap };

    fonts_set_fade_level(BODY_FONT, (uint8_t) level);
    for (int i = 0; i < 2; i++) {
        int x = centre - ((ink_x1[i] - ink_x0[i]) / 2) - ink_x0[i];
        ui_components_body_text_draw_shadowed(&(rdpq_textparms_t) { .style_id = STL_FADE }, x, baselines[i], lines[i], STL_FADE_SHADOW);
    }
    int bar_y = baselines[0] + POSITION_FRACTION_GAP;
    ui_components_box_draw(right - bar_width, bar_y, right, bar_y + POSITION_FRACTION_BAR, palette_mix(BACKGROUND_COLOR, TEXT_COLOR, level));
}
