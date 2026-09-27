/**
 * @file carousel_scroll.c
 * @brief Left / right scrolling for carousel lists, with letter paging while held
 * @ingroup ui_components
 *
 * Holding ←/→ moves one tile every CAROUSEL_REPEAT_FRAMES. After CAROUSEL_PAGING_DELAY_MS it
 * switches to jumping a letter at a time (A -> B -> C, landing on the first entry of each letter
 * in either direction) every CAROUSEL_PAGING_INTERVAL_MS, and shows the letter in the top-right
 * corner, fading in and out. Letters come from file names, which is what the lists are sorted by;
 * names that don't start with a letter are grouped as '#', and folders page separately from games.
 */

#include <ctype.h>
#include <string.h>

#include "../ui_components.h"
#include "../fonts.h"
#include "../sound.h"
#include "constants.h"
#include "utils/utils.h"

static struct {
    int hold_frames;
    uint32_t hold_start_ms;
    uint32_t last_page_ms;
    bool paging;
} scroll;

static struct {
    char letter;
    uint32_t shown_ms;      // when it started showing (fade in)
    uint32_t updated_ms;    // last page (fade out after this)
    bool visible;
} indicator;


static const char *base_name (const char *name) {
    const char *slash = strrchr(name, '/');
    return slash ? slash + 1 : name;
}

static char letter_of (entry_t *entry) {
    char c = toupper((unsigned char) base_name(entry->name)[0]);
    return (c >= 'A' && c <= 'Z') ? c : '#';
}

/** @brief Entries with the same key are one page: same type (folders first) and first letter. */
static int group_key (entry_t *entry) {
    return (entry->type << 8) | letter_of(entry);
}

/** @brief First entry of the run of equal keys that contains index (the list is circular). */
static int32_t group_start (entry_t *list, int32_t count, int32_t index) {
    int key = group_key(&list[index]);
    for (int32_t steps = 0; steps < count - 1; steps++) {
        int32_t previous = (index + count - 1) % count;
        if (group_key(&list[previous]) != key) {
            break;
        }
        index = previous;
    }
    return index;
}

/** @brief Index of the first entry of the next (direction 1) or previous (-1) letter. */
static int32_t page (entry_t *list, int32_t count, int32_t selected, int direction) {
    int key = group_key(&list[selected]);
    if (direction > 0) {
        for (int32_t steps = 1; steps < count; steps++) {
            int32_t index = (selected + steps) % count;
            if (group_key(&list[index]) != key) {
                return index;
            }
        }
        return selected;
    }
    int32_t start = group_start(list, count, selected);
    int32_t previous = (start + count - 1) % count;
    if (group_key(&list[previous]) == key) {
        return selected;    // the whole list is one letter
    }
    return group_start(list, count, previous);
}

/**
 * @brief Forget any held direction (call when a carousel view opens).
 */
void ui_components_carousel_scroll_reset (void) {
    scroll.hold_frames = 0;
    scroll.hold_start_ms = 0;
    scroll.paging = false;
}

/**
 * @brief Handle ←/→ for a carousel list (circular).
 *
 * @param letter_paging Whether a long hold pages by letter (lists sorted by file name).
 * @return The new selection.
 */
int32_t ui_components_carousel_scroll (menu_t *menu, entry_t *list, int32_t count, int32_t selected, bool letter_paging) {
    // C-buttons also report a direction (go_fast); in carousels they are action buttons instead.
    bool horizontal = (menu->actions.go_left || menu->actions.go_right) && !menu->actions.go_fast;
    if (!horizontal || count < 2) {
        ui_components_carousel_scroll_reset();
        return selected;
    }

    int direction = menu->actions.go_left ? -1 : 1;
    uint32_t now = get_ticks_ms();
    if (scroll.hold_frames == 0) {
        scroll.hold_start_ms = now;
    }

    if (letter_paging && (now - scroll.hold_start_ms) >= CAROUSEL_PAGING_DELAY_MS) {
        if (!scroll.paging || (now - scroll.last_page_ms) >= CAROUSEL_PAGING_INTERVAL_MS) {
            int32_t next = page(list, count, selected, direction);
            if (next != selected) {
                scroll.paging = true;
                scroll.last_page_ms = now;
                scroll.hold_frames++;
                if (!indicator.visible) {
                    indicator.shown_ms = now;
                }
                indicator.visible = true;
                indicator.letter = letter_of(&list[next]);
                indicator.updated_ms = now;
                sound_play_effect(SFX_CURSOR);
                return next;
            }
            // Only one letter in the list: keep scrolling tile by tile.
        } else {
            scroll.hold_frames++;
            return selected;
        }
    }

    // Held directions repeat every frame; throttle that to one tile every CAROUSEL_REPEAT_FRAMES.
    bool move_now = (scroll.hold_frames % CAROUSEL_REPEAT_FRAMES) == 0;
    scroll.hold_frames++;
    if (!move_now) {
        return selected;
    }
    sound_play_effect(SFX_CURSOR);
    return (selected + count + direction) % count;
}

/**
 * @brief Draw the letter being paged to in the top-right corner, fading in and out.
 */
void ui_components_letter_indicator_draw (void) {
    if (!indicator.visible) {
        return;
    }
    uint32_t now = get_ticks_ms();
    uint32_t since_shown = now - indicator.shown_ms;
    uint32_t since_update = now - indicator.updated_ms;

    int level = 0xFF;
    if (since_shown < LETTER_INDICATOR_FADE_IN_MS) {
        level = (since_shown * 0xFF) / LETTER_INDICATOR_FADE_IN_MS;
    }
    if (since_update > LETTER_INDICATOR_HOLD_MS) {
        uint32_t fading = since_update - LETTER_INDICATOR_HOLD_MS;
        if (fading >= LETTER_INDICATOR_FADE_OUT_MS) {
            indicator.visible = false;
            return;
        }
        level = MIN(level, (int) (((LETTER_INDICATOR_FADE_OUT_MS - fading) * 0xFF) / LETTER_INDICATOR_FADE_OUT_MS));
    }

    // The background is black, so fading the colour towards black fades the letter.
    fonts_set_fade_level(FNT_TITLE, (uint8_t) level);
    rdpq_text_printf(
        &(rdpq_textparms_t) { .style_id = STL_FADE, .width = LETTER_INDICATOR_WIDTH, .align = ALIGN_RIGHT },
        FNT_TITLE, VISIBLE_AREA_X1 - LETTER_INDICATOR_WIDTH, LETTER_INDICATOR_Y, "%c", indicator.letter
    );
}
