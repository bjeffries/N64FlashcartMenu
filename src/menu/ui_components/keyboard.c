/**
 * @file keyboard.c
 * @brief On-screen QWERTY keyboard dialog for entering short text
 * @ingroup ui_components
 *
 * D-pad moves over the keys, A types the selected key, B deletes a character, Z cancels and
 * Start (or the DONE key) finishes. SHIFT capitalises the next letter; it is on for the first
 * letter and turns itself off after typing one.
 */

#include <ctype.h>
#include <string.h>

#include "../ui_components.h"
#include "../fonts.h"
#include "../sound.h"
#include "constants.h"

#define COLUMNS         (10)
#define LETTER_ROWS     (4)
#define ROWS            (LETTER_ROWS + 1)
#define KEY_WIDTH       (40)
#define KEY_HEIGHT      (26)
#define KEY_GAP         (4)
#define MAX_TEXT        (63)

static const char *letter_rows[LETTER_ROWS] = {
    "1234567890",
    "qwertyuiop",
    "asdfghjkl-",
    "zxcvbnm.,'",
};

// Bottom row: wide keys, each spanning several columns.
typedef enum { KEY_SHIFT, KEY_SPACE, KEY_DELETE, KEY_DONE, SPECIAL_KEYS } special_key_t;
static const struct { const char *name; int first_column; int columns; } special_keys[SPECIAL_KEYS] = {
    [KEY_SHIFT] = { "SHIFT", 0, 2 },
    [KEY_SPACE] = { "SPACE", 2, 4 },
    [KEY_DELETE] = { "DEL", 6, 2 },
    [KEY_DONE] = { "DONE", 8, 2 },
};

static struct {
    bool open;
    const char *title;
    char text[MAX_TEXT + 1];
    int max_length;
    int row;
    int column;
    bool shift;
} keyboard;


static special_key_t special_key_at (int column) {
    for (int i = 0; i < SPECIAL_KEYS; i++) {
        if (column >= special_keys[i].first_column && column < special_keys[i].first_column + special_keys[i].columns) {
            return i;
        }
    }
    return KEY_DONE;
}

static void type_char (char c) {
    int length = strlen(keyboard.text);
    if (length >= keyboard.max_length) {
        sound_play_effect(SFX_ERROR);
        return;
    }
    if (isalpha((unsigned char) c)) {
        c = keyboard.shift ? toupper((unsigned char) c) : c;
        keyboard.shift = false;
    }
    keyboard.text[length] = c;
    keyboard.text[length + 1] = '\0';
    sound_play_effect(SFX_CURSOR);
}

static void delete_char (void) {
    int length = strlen(keyboard.text);
    if (length > 0) {
        keyboard.text[length - 1] = '\0';
        keyboard.shift = (length == 1);     // capitalise again when back at the start
        sound_play_effect(SFX_EXIT);
    }
}

/**
 * @brief Open the keyboard.
 *
 * @param title Dialog title (e.g. "Description").
 * @param initial Starting text (may be NULL).
 * @param max_length Maximum number of characters.
 */
void ui_components_keyboard_open (const char *title, const char *initial, int max_length) {
    keyboard.open = true;
    keyboard.title = title;
    keyboard.max_length = (max_length < MAX_TEXT) ? max_length : MAX_TEXT;
    snprintf(keyboard.text, (size_t) keyboard.max_length + 1, "%s", initial ? initial : "");
    keyboard.row = 1;
    keyboard.column = 0;
    keyboard.shift = (keyboard.text[0] == '\0');
}

/**
 * @brief Whether the keyboard is showing (the view should hand it all input).
 */
bool ui_components_keyboard_is_open (void) {
    return keyboard.open;
}

/**
 * @brief The text typed so far (valid after KEYBOARD_DONE until the keyboard is opened again).
 */
const char *ui_components_keyboard_text (void) {
    return keyboard.text;
}

/**
 * @brief Handle input while the keyboard is open.
 *
 * @return KEYBOARD_DONE or KEYBOARD_CANCELLED when it closes, otherwise KEYBOARD_EDITING.
 */
keyboard_result_t ui_components_keyboard_process (menu_t *menu) {
    if (!keyboard.open) {
        return KEYBOARD_CANCELLED;
    }

    if (menu->actions.go_up) {
        keyboard.row = (keyboard.row + ROWS - 1) % ROWS;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.go_down) {
        keyboard.row = (keyboard.row + 1) % ROWS;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.go_left || menu->actions.go_right) {
        int step = menu->actions.go_left ? -1 : 1;
        if (keyboard.row == LETTER_ROWS) {
            // Jump whole wide keys on the bottom row.
            special_key_t key = special_key_at(keyboard.column);
            key = (key + SPECIAL_KEYS + step) % SPECIAL_KEYS;
            keyboard.column = special_keys[key].first_column;
        } else {
            keyboard.column = (keyboard.column + COLUMNS + step) % COLUMNS;
        }
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.enter) {
        if (keyboard.row < LETTER_ROWS) {
            type_char(letter_rows[keyboard.row][keyboard.column]);
        } else {
            switch (special_key_at(keyboard.column)) {
                case KEY_SHIFT: keyboard.shift = !keyboard.shift; sound_play_effect(SFX_SETTING); break;
                case KEY_SPACE: type_char(' '); break;
                case KEY_DELETE: delete_char(); break;
                default:
                    keyboard.open = false;
                    sound_play_effect(SFX_ENTER);
                    return KEYBOARD_DONE;
            }
        }
    } else if (menu->actions.back) {
        delete_char();
    } else if (menu->actions.settings) {       // Start
        keyboard.open = false;
        sound_play_effect(SFX_ENTER);
        return KEYBOARD_DONE;
    } else if (menu->actions.lz_context && !menu->actions.tab_prev) {      // Z
        keyboard.open = false;
        sound_play_effect(SFX_EXIT);
        return KEYBOARD_CANCELLED;
    }
    return KEYBOARD_EDITING;
}

static void draw_key (int x, int y, int width, const char *label, bool selected, bool highlighted) {
    color_t background = selected ? RGBA32(0xFF, 0xFF, 0xFF, 0xFF) : (highlighted ? KEYBOARD_KEY_ACTIVE_COLOR : KEYBOARD_KEY_COLOR);
    ui_components_box_draw(x, y, x + width, y + KEY_HEIGHT, background);
    rdpq_text_printf(
        &(rdpq_textparms_t) { .style_id = selected ? STL_BLACK : STL_DEFAULT, .width = width, .align = ALIGN_CENTER },
        (strlen(label) > 1) ? FNT_SMALL : FNT_DEFAULT, x, y + ((strlen(label) > 1) ? 17 : 20), "%s", label
    );
}

/**
 * @brief Draw the keyboard dialog (does nothing when closed).
 */
void ui_components_keyboard_draw (void) {
    if (!keyboard.open) {
        return;
    }

    int keys_width = (COLUMNS * KEY_WIDTH) + ((COLUMNS - 1) * KEY_GAP);
    int keys_height = (ROWS * KEY_HEIGHT) + ((ROWS - 1) * KEY_GAP);
    int dialog_height = 84 + keys_height;
    ui_components_dialog_draw(keys_width + 32, dialog_height + 16);

    int x0 = DISPLAY_CENTER_X - (keys_width / 2);
    int y0 = DISPLAY_CENTER_Y - (dialog_height / 2);

    rdpq_text_printf(&(rdpq_textparms_t) { .style_id = STL_GRAY }, FNT_SMALL, x0, y0 + 10, "%s", keyboard.title);
    rdpq_text_printf(&(rdpq_textparms_t) { .style_id = STL_GRAY, .width = keys_width, .align = ALIGN_RIGHT },
        FNT_SMALL, x0, y0 + 10, "%d/%d", (int) strlen(keyboard.text), keyboard.max_length);

    // Text field with a cursor.
    ui_components_box_draw(x0, y0 + 20, x0 + keys_width, y0 + 50, KEYBOARD_FIELD_COLOR);
    rdpq_textmetrics_t metrics = rdpq_text_printf(NULL, FNT_DEFAULT, x0 + 8, y0 + 42, "%s", keyboard.text);
    int cursor_x = x0 + 8 + (int) (metrics.advance_x) + 1;
    ui_components_box_draw(cursor_x, y0 + 26, cursor_x + 2, y0 + 45, RGBA32(0xFF, 0xFF, 0xFF, 0xFF));

    int keys_y = y0 + 62;
    for (int row = 0; row < LETTER_ROWS; row++) {
        for (int column = 0; column < COLUMNS; column++) {
            char label[2] = { letter_rows[row][column], '\0' };
            if (keyboard.shift && isalpha((unsigned char) label[0])) {
                label[0] = toupper((unsigned char) label[0]);
            }
            int x = x0 + column * (KEY_WIDTH + KEY_GAP);
            int y = keys_y + row * (KEY_HEIGHT + KEY_GAP);
            draw_key(x, y, KEY_WIDTH, label, row == keyboard.row && column == keyboard.column, false);
        }
    }

    int y = keys_y + LETTER_ROWS * (KEY_HEIGHT + KEY_GAP);
    special_key_t selected_key = (keyboard.row == LETTER_ROWS) ? special_key_at(keyboard.column) : SPECIAL_KEYS;
    for (int i = 0; i < SPECIAL_KEYS; i++) {
        int x = x0 + special_keys[i].first_column * (KEY_WIDTH + KEY_GAP);
        int width = special_keys[i].columns * KEY_WIDTH + (special_keys[i].columns - 1) * KEY_GAP;
        draw_key(x, y, width, special_keys[i].name, i == selected_key, i == KEY_SHIFT && keyboard.shift);
    }

    ui_components_button_hints_draw((button_hint_t[]) {
        { ICON_A, "Type" },
        { ICON_B, "Delete" },
        { ICON_Z, "Cancel" },
    }, 3);
}
