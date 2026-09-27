/**
 * @file datel_code_editor.c
 * @brief Cheat Codes screen (Config > Cheat Codes): list, toggle and edit GameShark / Action Replay codes
 * @author Robin Jones (networkfusion)
 * @ingroup views
 *
 * A toggles a code (or starts entering one in an empty slot), C-Right edits it digit by digit,
 * C-Left names it with the on-screen keyboard, C-Up clears it. Leaving with B applies the codes and saves them next to the ROM (.datel).
 */

#include "../fonts.h"
#include "../ui_components/constants.h"
#include "../datel_codes.h"
#include "../sound.h"
#include "views.h"
#include "utils/fs.h"

#define CODE_DIGITS         (12)    // 8 address digits + 4 value digits
#define ADDRESS_DIGITS      (8)
#define DIGIT_WIDTH         (14)    // fixed cell per hex digit (title font) so codes line up
#define VALUE_GAP           (10)    // space between the address and the value
#define DESCRIPTION_GAP     (16)    // space between the value and the description
#define DESCRIPTION_MAX     (14)    // characters that fit in the description column
#define VISIBLE_ROWS        (8)

static cheat_file_code_t *cheat_codes;
static int selected = 0;
static int first_visible = 0;
static bool changed = false;

static bool confirm_clear = false;
static bool editing = false;
static int editing_digit = 0;
static cheat_file_code_t editing_backup;


static bool is_empty (cheat_file_code_t *code) {
    return code->address == 0 && code->value == 0 && code->description[0] == '\0';
}

static int get_digit (cheat_file_code_t *code, int digit) {
    if (digit < ADDRESS_DIGITS) {
        return (code->address >> ((ADDRESS_DIGITS - 1 - digit) * 4)) & 0xF;
    }
    return (code->value >> ((CODE_DIGITS - 1 - digit) * 4)) & 0xF;
}

static void set_digit (cheat_file_code_t *code, int digit, int nibble) {
    if (digit < ADDRESS_DIGITS) {
        int shift = (ADDRESS_DIGITS - 1 - digit) * 4;
        code->address = (code->address & ~(0xFu << shift)) | ((uint32_t) (nibble & 0xF) << shift);
    } else {
        int shift = (CODE_DIGITS - 1 - digit) * 4;
        code->value = (code->value & ~(0xFu << shift)) | ((uint16_t) (nibble & 0xF) << shift);
    }
}

static void start_editing (void) {
    editing_backup = cheat_codes[selected];
    editing_digit = 0;
    editing = true;
}

static void save_codes (menu_t *menu) {
    set_cheat_codes(cheat_codes);
    if (changed) {
        path_t *path = path_clone(menu->load.rom_path);
        path_ext_replace(path, "datel");
        save_cheats_to_file(path_get(path));
        path_free(path);
        changed = false;
    }
}

static void process_editing (menu_t *menu) {
    cheat_file_code_t *code = &cheat_codes[selected];

    if (menu->actions.go_left) {
        editing_digit = (editing_digit + CODE_DIGITS - 1) % CODE_DIGITS;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.go_right) {
        editing_digit = (editing_digit + 1) % CODE_DIGITS;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.go_up || menu->actions.go_down) {
        set_digit(code, editing_digit, get_digit(code, editing_digit) + (menu->actions.go_up ? 1 : -1));
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.enter) {
        code->enabled = true;
        editing = false;
        changed = true;
        sound_play_effect(SFX_SETTING);
        // A newly added code goes straight on to being named.
        if (is_empty(&editing_backup)) {
            ui_components_keyboard_open("Description", code->description, DESCRIPTION_MAX);
        }
    } else if (menu->actions.back) {
        *code = editing_backup;
        editing = false;
        sound_play_effect(SFX_EXIT);
    }
}

static void process (menu_t *menu) {
    // C-buttons also report a direction (go_fast); here they are action buttons, not movement.
    if (menu->actions.go_fast) {
        menu->actions.go_up = menu->actions.go_down = false;
        menu->actions.go_left = menu->actions.go_right = false;
    }

    cheat_file_code_t *code = &cheat_codes[selected];

    if (confirm_clear) {
        if (menu->actions.enter) {
            *code = (cheat_file_code_t) { 0 };
            changed = true;
            confirm_clear = false;
            sound_play_effect(SFX_SETTING);
        } else if (menu->actions.back) {
            confirm_clear = false;
            sound_play_effect(SFX_EXIT);
        }
        return;
    }

    if (ui_components_keyboard_is_open()) {
        if (ui_components_keyboard_process(menu) == KEYBOARD_DONE) {
            snprintf(code->description, sizeof(code->description), "%s", ui_components_keyboard_text());
            changed = true;
        }
        return;
    }

    if (editing) {
        process_editing(menu);
        return;
    }

    if (menu->actions.go_up) {
        selected = (selected + MAX_CHEAT_CODES - 1) % MAX_CHEAT_CODES;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.go_down) {
        selected = (selected + 1) % MAX_CHEAT_CODES;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.enter) {
        if (is_empty(code)) {
            start_editing();
            sound_play_effect(SFX_ENTER);
        } else {
            code->enabled = !code->enabled;
            changed = true;
            sound_play_effect(SFX_SETTING);
        }
    } else if (menu->actions.c_right) {
        start_editing();
        sound_play_effect(SFX_ENTER);
    } else if (menu->actions.c_left && !is_empty(code)) {
        ui_components_keyboard_open("Description", code->description, DESCRIPTION_MAX);
        sound_play_effect(SFX_ENTER);
    } else if (menu->actions.c_up && !is_empty(code)) {
        confirm_clear = true;
        sound_play_effect(SFX_SETTING);
    } else if (menu->actions.back) {
        save_codes(menu);
        menu->next_mode = MENU_MODE_LOAD_ROM;
        sound_play_effect(SFX_EXIT);
    }
}

/** @brief Draw a code as 8 + 4 hex digits in fixed cells; returns the x just after it. */
static int draw_code (cheat_file_code_t *code, int x, int y, bool row_selected, bool editing_row) {
    for (int digit = 0; digit < CODE_DIGITS; digit++) {
        int cx = x + (digit * DIGIT_WIDTH) + ((digit >= ADDRESS_DIGITS) ? VALUE_GAP : 0);
        bool digit_selected = editing_row && (digit == editing_digit);
        menu_font_style_t style = (editing_row ? digit_selected : row_selected) ? STL_DEFAULT : STL_GRAY;
        rdpq_text_printf(&(rdpq_textparms_t) { .style_id = style }, TITLE_FONT, cx, y, "%X", get_digit(code, digit));
        if (digit_selected) {
            ui_components_box_draw(cx - 1, y + 3, cx + DIGIT_WIDTH - 2, y + 5, EDIT_UNDERLINE_COLOR);
        }
    }
    return x + (CODE_DIGITS * DIGIT_WIDTH) + VALUE_GAP;
}

static void draw_column_headers (void) {
    // Between the game title and the first row.
    int y = CHEAT_LIST_Y - fonts_cap_height(TITLE_FONT) - 12;
    int x = CAROUSEL_SELECTED_X + 44;
    rdpq_textparms_t parms = { .style_id = STL_GRAY };
    ui_components_body_text_draw(&parms, CAROUSEL_SELECTED_X + 16, y, "#");
    ui_components_body_text_draw(&parms, x, y, "ADDRESS");
    ui_components_body_text_draw(&parms, x + (ADDRESS_DIGITS * DIGIT_WIDTH) + VALUE_GAP, y, "VALUE");
    ui_components_body_text_draw(&parms, x + (CODE_DIGITS * DIGIT_WIDTH) + VALUE_GAP + DESCRIPTION_GAP, y, "DESCRIPTION");
    ui_components_body_text_draw(&parms, CHEAT_STATE_X, y, "ENABLED");
}

static void draw_list (void) {
    draw_column_headers();

    if (selected < first_visible) {
        first_visible = selected;
    } else if (selected >= first_visible + VISIBLE_ROWS) {
        first_visible = selected - VISIBLE_ROWS + 1;
    }

    for (int row = 0; row < VISIBLE_ROWS && first_visible + row < MAX_CHEAT_CODES; row++) {
        int i = first_visible + row;
        cheat_file_code_t *code = &cheat_codes[i];
        int y = CHEAT_LIST_Y + (row * CHEAT_ROW_PITCH);
        bool is_selected = (i == selected);
        bool editing_row = editing && is_selected;

        if (is_selected) {
            int cap = fonts_cap_height(TITLE_FONT);
            ui_components_box_draw(CAROUSEL_SELECTED_X, y - cap - 3, CAROUSEL_SELECTED_X + 4, y + 3, SELECTION_MARKER_COLOR);
        }
        ui_components_body_text_printf(&(rdpq_textparms_t) { .style_id = STL_GRAY }, CAROUSEL_SELECTED_X + 16, y, "%02d", i + 1);

        int x = CAROUSEL_SELECTED_X + 44;
        if (is_empty(code) && !editing_row) {
            rdpq_text_printf(&(rdpq_textparms_t) { .style_id = STL_GRAY }, TITLE_FONT, x, y, "Empty");
            continue;
        }

        x = draw_code(code, x, y, is_selected, editing_row) + DESCRIPTION_GAP;

        if (code->description[0] != '\0') {
            ui_components_text_draw(
                &(rdpq_textparms_t) { .style_id = is_selected ? STL_DEFAULT : STL_GRAY, .width = CHEAT_STATE_X - x - 12, .wrap = WRAP_ELLIPSES },
                TITLE_FONT, x, y, code->description
            );
        }

        rdpq_text_printf(&(rdpq_textparms_t) { .style_id = code->enabled ? STL_DEFAULT : STL_GRAY },
            TITLE_FONT, CHEAT_STATE_X, y, "%s", code->enabled ? "On" : "Off");
    }

    ui_components_table_scrollbar_draw(CHEAT_LIST_Y, CHEAT_ROW_PITCH, first_visible, MAX_CHEAT_CODES, VISIBLE_ROWS);
}

static void draw (menu_t *menu, surface_t *display) {
    ui_components_attach_clear(display);

    char title[128];
    ui_components_carousel_title(path_last_get(menu->load.rom_path), false, title, sizeof(title));
    ui_components_screen_title_draw("Cheat Codes", title);

    draw_list();

    if (ui_components_keyboard_is_open()) {
        ui_components_keyboard_draw();      // draws its own button hints
    } else if (editing) {
        ui_components_body_text_draw(
            &(rdpq_textparms_t) { .style_id = STL_GRAY, .width = VISIBLE_AREA_X1 - CAROUSEL_SELECTED_X - 16 },
            CAROUSEL_SELECTED_X + 16, CHEAT_LIST_Y + (VISIBLE_ROWS * CHEAT_ROW_PITCH),
            "Left / Right: choose a digit. Up / Down: change it."
        );
        ui_components_button_hints_draw((button_hint_t[]) { { ICON_A, "Done" }, { ICON_B, "Cancel" } }, 2);
    } else {
        ui_components_body_text_draw(
            &(rdpq_textparms_t) { .style_id = STL_GRAY, .width = VISIBLE_AREA_X1 - CAROUSEL_SELECTED_X - 16, .wrap = WRAP_WORD },
            CAROUSEL_SELECTED_X + 16, CHEAT_LIST_Y + (VISIBLE_ROWS * CHEAT_ROW_PITCH),
            "Codes are applied when Cheats is On in Config. Changes are saved when you go back."
        );
        bool empty = is_empty(&cheat_codes[selected]);
        button_hint_t hints[BUTTON_HINTS_MAX] = {
            { ICON_A, empty ? "Add" : "Toggle" },
            { ICON_B, "Back" },
            { ICON_C_LEFT, "Name" },
            { ICON_C_UP, "Clear" },
            { ICON_C_RIGHT, "Edit" },
        };
        ui_components_button_hints_draw(hints, empty ? 2 : 5);
    }

    if (confirm_clear) {
        ui_components_messagebox_draw(
            "Clear this cheat code?\n\n"
            "A: Clear    B: Cancel"
        );
    }

    rdpq_detach_show();
}

void view_datel_code_editor_init (menu_t *menu) {
    if (!is_memory_expanded()) {
        menu_show_error(menu, "Cheat codes need an Expansion Pak");
        return;
    }

    editing = false;
    confirm_clear = false;
    changed = false;
    selected = 0;
    first_visible = 0;

    cheat_codes = get_cheat_codes();
    path_t *rom_datel_filepath = path_clone(menu->load.rom_path);
    path_ext_replace(rom_datel_filepath, "datel");

    path_t *rom_datel_txt_filepath = path_clone(menu->load.rom_path);
    path_ext_replace(rom_datel_txt_filepath, "datel.txt");

    if (file_exists(path_get(rom_datel_filepath))) {
        debugf("Cheat Editor: Loading cheats from %s.\n", path_get(rom_datel_filepath));
        load_cheats_from_file(path_get(rom_datel_filepath));
    } else if (file_exists(path_get(rom_datel_txt_filepath))) {
        debugf("Cheat Editor: Loading cheats from %s.\n", path_get(rom_datel_txt_filepath));
        load_cheats_from_file(path_get(rom_datel_txt_filepath));
    } else {
        debugf("Cheat Editor: No cheat file found, starting with empty list.\n");
    }

    path_free(rom_datel_filepath);
    path_free(rom_datel_txt_filepath);
}

void view_datel_code_editor_display (menu_t *menu, surface_t *display) {
    process(menu);

    draw(menu, display);
}
