/**
 * @file option_list.c
 * @brief Vertical option list in the Settings tab style
 * @ingroup ui_components
 *
 * Only rows that can be changed are selectable (up/down skip the rest); the selected row gets
 * the white bar. A flips toggles in place, opens the picker of a multiple-choice option, or runs
 * an action. Read-only information rows sit on a dark band and can't be selected.
 */

#include <string.h>

#include "../ui_components.h"
#include "../fonts.h"
#include "../sound.h"
#include "constants.h"
#include "utils/utils.h"


static bool is_selectable (option_t *option) {
    switch (option->type) {
        case OPTION_TOGGLE:
        case OPTION_CHOICE: return true;
        case OPTION_ACTION: return option->action != NULL;
        default: return false;
    }
}

/** @brief Next selectable row from `from` in `direction` (+1 / -1), or -1 if there is none. */
static int find_selectable (option_list_t *list, int from, int direction) {
    for (int i = from; i >= 0 && i < list->count; i += direction) {
        if (is_selectable(&list->options[i])) {
            return i;
        }
    }
    return -1;
}

static component_context_menu_t *open_picker (option_list_t *list) {
    if (list->selected < 0) {
        return NULL;
    }
    option_t *option = &list->options[list->selected];
    if (option->type == OPTION_CHOICE && option->picker && option->picker->row_selected >= 0) {
        return option->picker;
    }
    return NULL;
}

static const char *choice_text (menu_t *menu, option_t *option) {
    if (option->value) {
        return option->value(menu);
    }
    component_context_menu_t *picker = option->picker;
    int index = picker->get_default_selection ? picker->get_default_selection(menu) : 0;
    return picker->list[index].text;
}

/**
 * @brief Select the first changeable row and prepare the pickers; call from the view's init.
 */
void ui_components_option_list_init (option_list_t *list) {
    list->selected = find_selectable(list, 0, +1);
    list->first_visible = 0;
    for (int i = 0; i < list->count; i++) {
        if (list->options[i].picker) {
            ui_components_context_menu_init(list->options[i].picker);
        }
    }
}

/**
 * @brief Handle input for the list (and its open picker, if any).
 *
 * @return true if the input was used (the view should not handle it further).
 */
bool ui_components_option_list_process (menu_t *menu, option_list_t *list) {
    component_context_menu_t *picker = open_picker(list);
    if (picker) {
        ui_components_context_menu_process(menu, picker);
        return true;
    }
    if (list->selected < 0) {
        return false;       // nothing on this screen can be changed
    }

    if (menu->actions.go_up || menu->actions.go_down) {
        int next = find_selectable(list, list->selected + (menu->actions.go_up ? -1 : +1), menu->actions.go_up ? -1 : +1);
        if (next >= 0) {
            list->selected = next;
            sound_play_effect(SFX_CURSOR);
        }
        return true;
    }
    if (!menu->actions.enter) {
        return false;
    }

    option_t *option = &list->options[list->selected];
    switch (option->type) {
        case OPTION_TOGGLE:
            option->set(menu, !option->get(menu));
            sound_play_effect(SFX_SETTING);
            break;
        case OPTION_CHOICE:
            // Stand-alone pop-up (no parent, so B just closes it), opened on the current value.
            ui_components_context_menu_init(option->picker);
            ui_components_context_menu_show(option->picker);
            if (option->picker->get_default_selection) {
                option->picker->row_selected = option->picker->get_default_selection(menu);
            }
            sound_play_effect(SFX_SETTING);
            break;
        case OPTION_ACTION:
            option->action(menu);
            sound_play_effect(SFX_ENTER);
            break;
        default:
            break;
    }
    return true;
}

/**
 * @brief What A does on the selected row ("Toggle", "Change" or "Open"), or NULL if nothing is selectable.
 */
const char *ui_components_option_list_action_name (option_list_t *list) {
    if (list->selected < 0) {
        return NULL;
    }
    switch (list->options[list->selected].type) {
        case OPTION_TOGGLE: return "Toggle";
        case OPTION_CHOICE: return "Change";
        default: return list->options[list->selected].action_name ? list->options[list->selected].action_name : "Open";
    }
}

void ui_components_table_scrollbar_draw (int first_row_y, int pitch, int first_visible, int count, int visible) {
    if (count <= visible) {
        return;
    }
    int top = first_row_y - fonts_cap_height(TITLE_FONT) - 3;
    int bottom = first_row_y + ((visible - 1) * pitch) + 3;
    int height = bottom - top;
    int thumb = MAX(TABLE_SCROLLBAR_MIN_THUMB, (height * visible) / count);
    int thumb_y = top + ((height - thumb) * first_visible) / (count - visible);
    ui_components_box_draw(TABLE_SCROLLBAR_X, top, VISIBLE_AREA_X1, bottom, SCROLLBAR_BG_COLOR);
    ui_components_box_draw(TABLE_SCROLLBAR_X, thumb_y, VISIBLE_AREA_X1, thumb_y + thumb, SCROLLBAR_POSITION_COLOR);
}

/**
 * @brief Draw the rows between y_top and y_bottom (baselines), the selected row's description
 *        just below the last row, and the open picker on top.
 */
void ui_components_option_list_draw (menu_t *menu, option_list_t *list, int y_top, int y_bottom) {
    int pitch = OPTION_LIST_ROW_PITCH;
    int visible = MAX(1, (y_bottom - y_top) / pitch + 1);

    // Keep the selected row on screen.
    if (list->selected >= 0) {
        if (list->selected < list->first_visible) {
            list->first_visible = list->selected;
        } else if (list->selected >= list->first_visible + visible) {
            list->first_visible = list->selected - visible + 1;
        }
    }

    int cap = fonts_cap_height(TITLE_FONT);
    bool scrolls = list->count > visible;
    int band_x1 = scrolls ? TABLE_SCROLLBAR_X - 6 : VISIBLE_AREA_X1;    // stop short of the scroll bar
    int rows_drawn = 0;
    for (int row = 0; row < visible && list->first_visible + row < list->count; row++, rows_drawn++) {
        int i = list->first_visible + row;
        option_t *option = &list->options[i];
        int y = y_top + (row * pitch);
        bool is_selected = (i == list->selected);
        bool selectable = is_selectable(option);

        // Rows are title-style text; the band and marker span its capitals plus a margin.
        if (!selectable) {
            ui_components_box_draw(CAROUSEL_SELECTED_X, y - cap - 6, band_x1, y + 6, OPTION_LIST_INFO_BAND_COLOR);
        } else if (is_selected) {
            ui_components_box_draw(CAROUSEL_SELECTED_X, y - cap - 3, CAROUSEL_SELECTED_X + 4, y + 3, SELECTION_MARKER_COLOR);
        }
        rdpq_text_printf(
            &(rdpq_textparms_t) { .style_id = is_selected ? STL_DEFAULT : STL_GRAY },
            TITLE_FONT, CAROUSEL_SELECTED_X + 16, y, "%s", option->label
        );

        const char *value = NULL;
        menu_font_style_t value_style = STL_DEFAULT;
        if (option->type == OPTION_TOGGLE) {
            bool on = option->get(menu);
            value = on ? "On" : "Off";
            value_style = on ? STL_DEFAULT : STL_GRAY;
        } else if (option->type == OPTION_CHOICE) {
            value = choice_text(menu, option);
        } else if (option->value) {
            value = option->value(menu);
            value_style = selectable ? STL_GRAY : STL_DEFAULT;
        }
        if (value) {
            ui_components_text_draw(
                &(rdpq_textparms_t) { .style_id = value_style, .width = VISIBLE_AREA_X1 - OPTION_LIST_VALUE_X - 8, .wrap = WRAP_ELLIPSES },
                TITLE_FONT, OPTION_LIST_VALUE_X, y, value
            );
        }
    }

    // Scroll hints when rows are off screen.
    ui_components_table_scrollbar_draw(y_top, pitch, list->first_visible, list->count, visible);

    if (list->selected >= 0 && list->options[list->selected].description) {
        ui_components_body_text_draw(
            &(rdpq_textparms_t) { .style_id = STL_GRAY, .width = VISIBLE_AREA_X1 - CAROUSEL_SELECTED_X - 16, .wrap = WRAP_WORD },
            CAROUSEL_SELECTED_X + 16, y_top + (rows_drawn * pitch),
            list->options[list->selected].description
        );
    }

    component_context_menu_t *picker = open_picker(list);
    if (picker) {
        ui_components_context_menu_draw(picker);
    }
}

/**
 * @brief Draw a whole option-list screen: header, rows, and the A / B button hints.
 *        The view still attaches / detaches the display and draws any dialogs on top.
 */
void ui_components_option_screen_draw (menu_t *menu, const char *title, option_list_t *list) {
    rdpq_text_printf(NULL, TITLE_FONT, CAROUSEL_SELECTED_X, LIBRARY_HEADER_Y, "%s", title);

    ui_components_option_list_draw(menu, list, SETTINGS_LIST_Y, SETTINGS_LIST_Y + (OPTION_LIST_ROW_PITCH * (OPTION_LIST_VISIBLE_ROWS - 1)));

    button_hint_t hints[BUTTON_HINTS_MAX];
    int count = 0;
    const char *action = ui_components_option_list_action_name(list);
    if (action) {
        hints[count++] = (button_hint_t) { ICON_A, action };
    }
    hints[count++] = (button_hint_t) { ICON_B, "Back" };
    ui_components_button_hints_draw(hints, count);
}
