/**
 * @file common.c
 * @brief Common UI components implementation
 * @ingroup ui_components
 */

#include <stdarg.h>

#include "../ui_components.h"
#include "../fonts.h"
#include "constants.h"

/**
 * @brief Draw a box with the specified color.
 * 
 * @param x0 The x-coordinate of the top-left corner.
 * @param y0 The y-coordinate of the top-left corner.
 * @param x1 The x-coordinate of the bottom-right corner.
 * @param y1 The y-coordinate of the bottom-right corner.
 * @param color The color of the box.
 */
void ui_components_attach_clear (surface_t *d) {
    rdpq_attach(d, NULL);
    rdpq_clear(BACKGROUND_COLOR);
}

void ui_components_box_draw (int x0, int y0, int x1, int y1, color_t color) {
    rdpq_mode_push();
        rdpq_set_mode_fill(color);
        rdpq_fill_rectangle(x0, y0, x1, y1);
    rdpq_mode_pop();
}

/**
 * @brief Draw text that may be truncated with an ellipsis.
 *
 * rdpq_text_printn() lays text out in a stack buffer with no spare slot for its end marker,
 * so an ellipsis that lands near the end of a string overflows it and crashes. Building the
 * paragraph on the heap avoids that.
 *
 * @param parms Text parameters (usually with .wrap = WRAP_ELLIPSES).
 * @param font Font ID.
 * @param x Left edge.
 * @param y Baseline (or top edge if parms->height is set).
 * @param text UTF-8 text.
 */
void ui_components_screen_title_draw (const char *name, const char *subtitle) {
    rdpq_textmetrics_t metrics = rdpq_text_printf(&(rdpq_textparms_t) { .style_id = STL_DEFAULT },
        TITLE_FONT, CAROUSEL_SELECTED_X, LIBRARY_HEADER_Y, "%s", name);
    if (subtitle && subtitle[0] != '\0') {
        int x = CAROUSEL_SELECTED_X + (int) (metrics.advance_x);
        char text[160];
        snprintf(text, sizeof(text), "  >  %s", subtitle);
        ui_components_text_draw(
            &(rdpq_textparms_t) { .style_id = STL_GRAY, .width = VISIBLE_AREA_X1 - x, .wrap = WRAP_ELLIPSES },
            TITLE_FONT, x, LIBRARY_HEADER_Y, text
        );
    }
}

void ui_components_body_text_draw (const rdpq_textparms_t *parms, int x, int y, const char *text) {
    ui_components_body_text_draw_shadowed(parms, x, y, text, STL_SHADOW);
}

void ui_components_body_text_draw_shadowed (const rdpq_textparms_t *parms, int x, int y, const char *text, menu_font_style_t shadow_style) {
    rdpq_textparms_t shadow = parms ? *parms : (rdpq_textparms_t) { 0 };
    menu_font_style_t style = shadow.style_id;
    shadow.style_id = shadow_style;
    ui_components_text_draw(&shadow, BODY_FONT, x + TEXT_SHADOW_OFFSET, y + TEXT_SHADOW_OFFSET, text);
    shadow.style_id = style;
    ui_components_text_draw(&shadow, BODY_FONT, x, y, text);
}

void ui_components_body_text_printf (const rdpq_textparms_t *parms, int x, int y, const char *fmt, ...) {
    char text[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    ui_components_body_text_draw(parms, x, y, text);
}

void ui_components_text_draw (const rdpq_textparms_t *parms, menu_font_type_t font, int x, int y, const char *text) {
    int nbytes = strlen(text);
    rdpq_paragraph_t *layout = rdpq_paragraph_build(parms, font, text, &nbytes);
    rdpq_paragraph_render(layout, x, y);
    rdpq_paragraph_free(layout);
}

/**
 * @brief Full loading screen: a header message, the game's title and a progress bar with percentage.
 *
 * @param d Display surface (attached and shown by this function).
 * @param progress 0.0 to 1.0.
 * @param message Header text, e.g. "Loading".
 * @param file_name File name of the game or disk; shown as a cleaned-up title.
 */
void ui_components_loading_screen_draw (surface_t *d, float progress, const char *message, const char *file_name) {
    ui_components_attach_clear(d);

    rdpq_text_printf(NULL, TITLE_FONT, CAROUSEL_SELECTED_X, LIBRARY_HEADER_Y, "%s", message);

    char title[128];
    ui_components_carousel_title(file_name, false, title, sizeof(title));
    ui_components_text_draw(
        &(rdpq_textparms_t) { .style_id = STL_DEFAULT },    // long titles run off the edge of the screen
        FNT_TITLE, CAROUSEL_SELECTED_X, LOADING_TITLE_Y, title
    );

    ui_components_progressbar_draw(CAROUSEL_SELECTED_X, LOADING_BAR_Y, VISIBLE_AREA_X1, LOADING_BAR_Y + 6, progress);
    ui_components_body_text_printf(&(rdpq_textparms_t) { .style_id = STL_GRAY }, CAROUSEL_SELECTED_X, LOADING_BAR_Y + 24, "%d%%", (int) (progress * 100.0f));

    rdpq_detach_show();
}

/**
 * @brief Draw a border with the specified color.
 * 
 * @param x0 The x-coordinate of the top-left corner.
 * @param y0 The y-coordinate of the top-left corner.
 * @param x1 The x-coordinate of the bottom-right corner.
 * @param y1 The y-coordinate of the bottom-right corner.
 * @param color The color of the border.
 */
static void ui_components_border_draw_internal (int x0, int y0, int x1, int y1, color_t color) {
    rdpq_mode_push();
        rdpq_set_mode_fill(color);
        rdpq_fill_rectangle(x0 - BORDER_THICKNESS, y0 - BORDER_THICKNESS, x1 + BORDER_THICKNESS, y0);
        rdpq_fill_rectangle(x0 - BORDER_THICKNESS, y1, x1 + BORDER_THICKNESS, y1 + BORDER_THICKNESS);
        rdpq_fill_rectangle(x0 - BORDER_THICKNESS, y0, x0, y1);
        rdpq_fill_rectangle(x1, y0, x1 + BORDER_THICKNESS, y1);
    rdpq_mode_pop();
}

/**
 * @brief Draw a border with the default border color.
 * 
 * @param x0 The x-coordinate of the top-left corner.
 * @param y0 The y-coordinate of the top-left corner.
 * @param x1 The x-coordinate of the bottom-right corner.
 * @param y1 The y-coordinate of the bottom-right corner.
 */
void ui_components_border_draw (int x0, int y0, int x1, int y1) {
    ui_components_border_draw_internal(x0, y0, x1, y1, BORDER_COLOR);
}

/**
 * @brief Draw the layout with tabs (same as ui_components_layout_draw in the Library style).
 */
void ui_components_layout_draw_tabbed (void) {
    ui_components_layout_draw();
}

/**
 * @brief Draw the layout: no frame, just a thin line above the button hints.
 */
void ui_components_layout_draw (void) {
    ui_components_box_draw(
        VISIBLE_AREA_X0,
        LAYOUT_ACTIONS_SEPARATOR_Y,
        VISIBLE_AREA_X1,
        LAYOUT_ACTIONS_SEPARATOR_Y + 1,
        LAYOUT_SEPARATOR_COLOR
    );
}

/**
 * @brief Draw a progress bar.
 * 
 * @param x0 The x-coordinate of the top-left corner.
 * @param y0 The y-coordinate of the top-left corner.
 * @param x1 The x-coordinate of the bottom-right corner.
 * @param y1 The y-coordinate of the bottom-right corner.
 * @param progress The progress value (0.0 to 1.0).
 */
void ui_components_progressbar_draw (int x0, int y0, int x1, int y1, float progress) {    
    float progress_width = progress * (x1 - x0);

    ui_components_box_draw(x0, y0, x0 + progress_width, y1, PROGRESSBAR_DONE_COLOR);
    ui_components_box_draw(x0 + progress_width, y0, x1, y1, PROGRESSBAR_BG_COLOR);
}

/**
 * @brief Draw a seek bar.
 * 
 * @param position The position value (0.0 to 1.0).
 */
void ui_components_seekbar_draw (float position) {
    int x0 = SEEKBAR_X;
    int y0 = SEEKBAR_Y;
    int x1 = SEEKBAR_X + SEEKBAR_WIDTH;
    int y1 = SEEKBAR_Y + SEEKBAR_HEIGHT;

    ui_components_progressbar_draw(x0, y0, x1, y1, position);
}

/**
 * @brief Draw a loader.
 * 
 * @param progress The progress value (0.0 to 1.0).
 * @param msg The message to display truncated to 30 characters.
 */
void ui_components_loader_draw (float progress, const char *msg) {
    int x0 = LOADER_X;
    int y0 = LOADER_Y;
    int x1 = LOADER_X + LOADER_WIDTH;
    int y1 = LOADER_Y + LOADER_HEIGHT;

    ui_components_progressbar_draw(x0, y0, x1, y1, progress);

    if (msg != NULL) {
        ui_components_main_text_draw(
            STL_DEFAULT,
            ALIGN_CENTER, VALIGN_CENTER,
            "\n%.30s",
            msg
        );
    }
}

/**
 * @brief Draw a scrollbar.
 * 
 * @param x The x-coordinate of the top-left corner.
 * @param y The y-coordinate of the top-left corner.
 * @param width The width of the scrollbar.
 * @param height The height of the scrollbar.
 * @param position The current position.
 * @param items The total number of items.
 * @param visible_items The number of visible items.
 */
void ui_components_scrollbar_draw (int x, int y, int width, int height, int position, int items, int visible_items) {
    if (items <= 1 || items <= visible_items) {
        ui_components_box_draw(x, y, x + width, y + height, SCROLLBAR_INACTIVE_COLOR);
    } else {
        int scroll_height = (int) ((visible_items / (float) (items)) * height);
        float scroll_position = ((position / (float) (items - 1)) * (height - scroll_height));

        ui_components_box_draw(x, y, x + width, y + height, SCROLLBAR_BG_COLOR);
        ui_components_box_draw(x, y + scroll_position, x + width, y + scroll_position + scroll_height, SCROLLBAR_POSITION_COLOR);
    }
}

/**
 * @brief Draw a list scrollbar.
 * 
 * @param position The current position.
 * @param items The total number of items.
 * @param visible_items The number of visible items.
 */
void ui_components_list_scrollbar_draw (int position, int items, int visible_items) {
    ui_components_scrollbar_draw(
        LIST_SCROLLBAR_X,
        LIST_SCROLLBAR_Y,
        LIST_SCROLLBAR_WIDTH,
        LIST_SCROLLBAR_HEIGHT,
        position,
        items,
        visible_items
    );
}

/**
 * @brief Draw a dialog box.
 * 
 * @param width The width of the dialog box.
 * @param height The height of the dialog box.
 */
void ui_components_dialog_draw (int width, int height) {
    int x0 = DISPLAY_CENTER_X - (width / 2);
    int y0 = DISPLAY_CENTER_Y - (height / 2);
    int x1 = DISPLAY_CENTER_X + (width / 2);
    int y1 = DISPLAY_CENTER_Y + (height / 2);
    int t = DIALOG_BORDER;

    // Panel with a white outline, its corner pixels left out so the corners read as rounded.
    ui_components_box_draw(x0 - t + 1, y0 - t, x1 + t - 1, y1 + t, DIALOG_BORDER_COLOR);
    ui_components_box_draw(x0 - t, y0 - t + 1, x1 + t, y1 + t - 1, DIALOG_BORDER_COLOR);
    ui_components_box_draw(x0, y0, x1, y1, DIALOG_BG_COLOR);
}

/**
 * @brief Draw a message box with formatted text.
 * 
 * @param fmt The format string.
 * @param ... The format arguments.
 */
void ui_components_messagebox_draw (char *fmt, ...) {
    char buffer[512];
    size_t nbytes = sizeof(buffer);

    va_list va;
    va_start(va, fmt);
    char *formatted = vasnprintf(buffer, &nbytes, fmt, va);
    va_end(va);

    // Body text with its shadow: laid out twice (shadow style, then text style).
    rdpq_textparms_t parms = {
        .width = MESSAGEBOX_MAX_WIDTH,
        .height = VISIBLE_AREA_HEIGHT,
        .align = ALIGN_CENTER,
        .valign = VALIGN_CENTER,
        .wrap = WRAP_WORD,
        .line_spacing = TEXT_LINE_SPACING_ADJUST,
        .style_id = STL_SHADOW,
    };
    int paragraph_nbytes = nbytes;
    rdpq_paragraph_t *shadow = rdpq_paragraph_build(&parms, BODY_FONT, formatted, &paragraph_nbytes);
    parms.style_id = STL_DEFAULT;
    paragraph_nbytes = nbytes;
    rdpq_paragraph_t *paragraph = rdpq_paragraph_build(&parms, BODY_FONT, formatted, &paragraph_nbytes);

    if (formatted != buffer) {
        free(formatted);
    }

    ui_components_dialog_draw(
        paragraph->bbox.x1 - paragraph->bbox.x0 + MESSAGEBOX_MARGIN,
        paragraph->bbox.y1 - paragraph->bbox.y0 + MESSAGEBOX_MARGIN
    );

    // Centred on the screen like the dialog (the safe area isn't: its top margin is wider).
    int x = DISPLAY_CENTER_X - (MESSAGEBOX_MAX_WIDTH / 2);
    int y = DISPLAY_CENTER_Y - (VISIBLE_AREA_HEIGHT / 2);
    rdpq_paragraph_render(shadow, x + TEXT_SHADOW_OFFSET, y + TEXT_SHADOW_OFFSET);
    rdpq_paragraph_free(shadow);
    rdpq_paragraph_render(paragraph, x, y);

    rdpq_paragraph_free(paragraph);
}

/**
 * @brief Draw the main text with formatted content.
 * 
 * @param style The font style.
 * @param align The horizontal alignment.
 * @param valign The vertical alignment.
 * @param fmt The format string.
 * @param ... The format arguments.
 */
void ui_components_main_text_draw (menu_font_type_t style, rdpq_align_t align, rdpq_valign_t valign, char *fmt, ...) {
    char buffer[1024];
    size_t nbytes = sizeof(buffer);

    va_list va;
    va_start(va, fmt);
    char *formatted = vasnprintf(buffer, &nbytes, fmt, va);
    va_end(va);

    rdpq_text_printn(
        &(rdpq_textparms_t) {
            .style_id = style,
            .width = VISIBLE_AREA_WIDTH - (TEXT_MARGIN_HORIZONTAL * 2),
            .height = LAYOUT_ACTIONS_SEPARATOR_Y - VISIBLE_AREA_Y0 - (TEXT_MARGIN_VERTICAL * 2),
            .align = align,
            .valign = valign,
            .wrap = WRAP_WORD,
            .line_spacing = TEXT_LINE_SPACING_ADJUST,
        },
        BODY_FONT,
        VISIBLE_AREA_X0 + TEXT_MARGIN_HORIZONTAL,
        VISIBLE_AREA_Y0 + TEXT_MARGIN_VERTICAL + TEXT_OFFSET_VERTICAL,
        formatted,
        nbytes
    );

    if (formatted != buffer) {
        free(formatted);
    }
}

/**
 * @brief Draw the actions bar text with formatted content.
 * 
 * @param style The font style.
 * @param align The horizontal alignment.
 * @param valign The vertical alignment.
 * @param fmt The format string.
 * @param ... The format arguments.
 */
void ui_components_actions_bar_text_draw (menu_font_type_t style, rdpq_align_t align, rdpq_valign_t valign, char *fmt, ...) {
    char buffer[256];
    size_t nbytes = sizeof(buffer);

    va_list va;
    va_start(va, fmt);
    char *formatted = vasnprintf(buffer, &nbytes, fmt, va);
    va_end(va);

    ui_components_text_draw(
        &(rdpq_textparms_t) {
            .style_id = style,
            .width = VISIBLE_AREA_WIDTH - (TEXT_MARGIN_HORIZONTAL * 2),
            .height = VISIBLE_AREA_Y1 - LAYOUT_ACTIONS_SEPARATOR_Y - BORDER_THICKNESS - (TEXT_MARGIN_VERTICAL * 2),
            .align = align,
            .valign = valign,
            .wrap = WRAP_ELLIPSES,
            .line_spacing = TEXT_LINE_SPACING_ADJUST,
        },
        BODY_FONT,
        VISIBLE_AREA_X0 + TEXT_MARGIN_HORIZONTAL,
        LAYOUT_ACTIONS_SEPARATOR_Y + BORDER_THICKNESS + TEXT_MARGIN_VERTICAL + TEXT_OFFSET_VERTICAL,
        formatted
    );

    if (formatted != buffer) {
        free(formatted);
    }
}

/**
 * @brief Draw the tabs.
 * 
 * @param text Array of tab text.
 * @param count Number of tabs.
 * @param selected Index of the selected tab.
 * @param width Width of each tab.
 */
void ui_components_tabs_draw(const char **text, int count, int selected, float width ) {
    float starting_x = VISIBLE_AREA_X0;

    float x = starting_x;
    float y = VISIBLE_AREA_Y0;    
    float height = TAB_HEIGHT;

    // first draw the tabs that are not selected
    for(int i=0;i< count;i++) {
        if(i != selected) {
            ui_components_box_draw(
                x,
                y,
                x + width,
                y + height,
                TAB_INACTIVE_BACKGROUND_COLOR
            );

            ui_components_border_draw_internal(
                x,
                y,
                x + width,
                y + height,
                TAB_INACTIVE_BORDER_COLOR
            );
        }
        x += width;
    }
    
    // draw the selected tab (so it shows up on top of the others)
    if(selected >= 0 && selected < count) {
        x = starting_x + (width * selected);

        ui_components_box_draw(
            x,
            y,
            x + width,
            y + height,
            TAB_ACTIVE_BACKGROUND_COLOR
        );

        ui_components_border_draw_internal(
            x,
            y,
            x + width,
            y + height,
            TAB_ACTIVE_BORDER_COLOR
        );
    }

    // write the text on the tabs
    rdpq_textparms_t tab_textparms = {
        .width = width,
        .height = 24,
        .align = ALIGN_CENTER,
        .wrap = WRAP_NONE
    };
    x = starting_x;
    for(int i=0;i< count;i++) {
        rdpq_text_print(
            &tab_textparms,
            BODY_FONT,
            x,
            y,
            text[i]
        );
        x += width;
    }
}

void ui_component_value_editor(const char **header_text, const char **value_text, int count, int selected, float width_adjustment ) {
    float field_width = (VISIBLE_AREA_WIDTH - (TEXT_MARGIN_HORIZONTAL * 2)) / width_adjustment;
    float starting_x = DISPLAY_CENTER_X - (field_width * count / 2.0f);

    float x = starting_x;
    float y = DISPLAY_CENTER_Y;    
    float height = TAB_HEIGHT;

    // first draw the values that are not selected
    for(int i=0;i< count;i++) {
        if(i != selected) {
            ui_components_box_draw(
                x,
                y,
                x + field_width,
                y + height + 24,
                TAB_INACTIVE_BACKGROUND_COLOR
            );
        }
        x += field_width;
    }
    
    // draw the selected value (so it shows up on top of the others)
    if(selected >= 0 && selected < count) {
        x = starting_x + (field_width * selected);

        ui_components_box_draw(
            x,
            y,
            x + field_width,
            y + height + 24,
            TAB_ACTIVE_BACKGROUND_COLOR
        );
    }

    // write the text on the value boxes
    rdpq_textparms_t value_textparms = {
        .width = field_width,
        .height = 24,
        .align = ALIGN_CENTER,
        .wrap = WRAP_NONE
    };
    x = starting_x;
    for(int i=0;i< count;i++) {
        rdpq_text_print(
            &value_textparms,
            BODY_FONT,
            x,
            y,
            header_text[i]
        );

        rdpq_text_print(
            &value_textparms,
            BODY_FONT,
            x,
            y + 24,
            value_text[i]
        );
        x += field_width;
    }

    // draw the border around the value boxes
    ui_components_border_draw (starting_x, y, x, y + height + 24);
}
