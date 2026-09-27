/**
 * @file option_list.c
 * @brief Vertical option list in the Settings tab style
 * @ingroup ui_components
 *
 * Rows are scrolled with up/down; the selected row gets the white bar. A flips toggles in
 * place, opens the picker of a multiple-choice option, or runs an action.
 */

#include <string.h>

#include "../ui_components.h"
#include "../fonts.h"
#include "../sound.h"
#include "constants.h"
#include "utils/utils.h"


static component_context_menu_t *open_picker (option_list_t *list) {
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
 * @brief Reset the selection and prepare the pickers; call from the view's init.
 */
void ui_components_option_list_init (option_list_t *list) {
    list->selected = 0;
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

    if (menu->actions.go_up && list->selected > 0) {
        list->selected--;
        sound_play_effect(SFX_CURSOR);
        return true;
    }
    if (menu->actions.go_down && list->selected < list->count - 1) {
        list->selected++;
        sound_play_effect(SFX_CURSOR);
        return true;
    }
    if (!menu->actions.enter || list->count == 0) {
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
    }
    return true;
}

/**
 * @brief What A does on the selected row ("Toggle", "Change" or "Open"), for the button hints.
 */
const char *ui_components_option_list_action_name (option_list_t *list) {
    if (list->count == 0) {
        return NULL;
    }
    switch (list->options[list->selected].type) {
        case OPTION_TOGGLE: return "Toggle";
        case OPTION_CHOICE: return "Change";
        default: return "Open";
    }
}

/**
 * @brief Draw the rows between y_top and y_bottom (baselines), the selected row's description
 *        below them, and the open picker on top.
 */
void ui_components_option_list_draw (menu_t *menu, option_list_t *list, int y_top, int y_bottom) {
    int visible = MAX(1, (y_bottom - y_top) / OPTION_LIST_ROW_PITCH + 1);

    // Keep the selected row on screen.
    if (list->selected < list->first_visible) {
        list->first_visible = list->selected;
    } else if (list->selected >= list->first_visible + visible) {
        list->first_visible = list->selected - visible + 1;
    }

    for (int row = 0; row < visible && list->first_visible + row < list->count; row++) {
        int i = list->first_visible + row;
        option_t *option = &list->options[i];
        int y = y_top + (row * OPTION_LIST_ROW_PITCH);
        bool is_selected = (i == list->selected);

        if (is_selected) {
            ui_components_box_draw(CAROUSEL_SELECTED_X, y - 16, CAROUSEL_SELECTED_X + 4, y + 4, RGBA32(0xFF, 0xFF, 0xFF, 0xFF));
        }
        rdpq_text_printf(
            &(rdpq_textparms_t) { .style_id = is_selected ? STL_DEFAULT : STL_GRAY },
            FNT_DEFAULT, CAROUSEL_SELECTED_X + 16, y, "%s", option->label
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
            value_style = STL_GRAY;
        }
        if (value) {
            ui_components_text_draw(
                &(rdpq_textparms_t) { .style_id = value_style, .width = VISIBLE_AREA_X1 - OPTION_LIST_VALUE_X, .wrap = WRAP_ELLIPSES },
                FNT_DEFAULT, OPTION_LIST_VALUE_X, y, value
            );
        }
    }

    // Scroll hints when rows are off screen.
    if (list->first_visible > 0) {
        rdpq_text_printf(&(rdpq_textparms_t) { .style_id = STL_GRAY }, FNT_SMALL, VISIBLE_AREA_X1 - 12, y_top - 22, "...");
    }
    if (list->first_visible + visible < list->count) {
        rdpq_text_printf(&(rdpq_textparms_t) { .style_id = STL_GRAY }, FNT_SMALL, VISIBLE_AREA_X1 - 12, y_top + (visible * OPTION_LIST_ROW_PITCH) - 10, "...");
    }

    if (list->count > 0 && list->options[list->selected].description) {
        ui_components_text_draw(
            &(rdpq_textparms_t) { .style_id = STL_GRAY, .width = VISIBLE_AREA_X1 - CAROUSEL_SELECTED_X - 16, .wrap = WRAP_WORD },
            FNT_SMALL, CAROUSEL_SELECTED_X + 16, y_bottom + OPTION_LIST_ROW_PITCH,
            list->options[list->selected].description
        );
    }

    component_context_menu_t *picker = open_picker(list);
    if (picker) {
        ui_components_context_menu_draw(picker);
    }
}
