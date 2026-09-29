/**
 * @file ui_components.h
 * @brief Menu Graphical User Interface Components
 * @ingroup menu
 */

#ifndef UI_COMPONENTS_H__
#define UI_COMPONENTS_H__

#include <libdragon.h>
#include "menu_state.h"
#include "fonts.h"
#include "ui_components/palette.h"


/** 
 * @brief File image Enumeration.
 * 
 * Enumeration for different types of file images used in the user interface.
 */
typedef enum {
    IMAGE_BOXART_FRONT,    /**< Boxart image from the front */
    IMAGE_BOXART_BACK,     /**< Boxart image from the back */
    IMAGE_BOXART_TOP,      /**< Boxart image from the top */
    IMAGE_BOXART_BOTTOM,   /**< Boxart image from the bottom */
    IMAGE_BOXART_LEFT,     /**< Boxart image from the left side */
    IMAGE_BOXART_RIGHT,    /**< Boxart image from the right side */
    IMAGE_GAMEPAK_FRONT,   /**< GamePak image from the front */
    IMAGE_GAMEPAK_BACK,    /**< GamePak image from the back */
    IMAGE_THUMBNAIL,       /**< File image thumbnail */
    IMAGE_TYPE_END         /**< List end marker */
} file_image_type_t;


typedef struct fat_file_attributes {
    bool is_read_only;    /**< Read-only attribute */
    bool is_hidden;       /**< Hidden attribute */
    bool is_system;       /**< System attribute */
    bool is_archive;      /**< Archive attribute */
} fat_file_attributes_t;


typedef struct pak_file_attributes {
    bool is_controller_pak_dump;                /**< file is a controller pak dump */
    bool is_controller_pak_dump_note;           /**< file is a controller pak dump note */
} pak_file_attributes_t;


/** 
 * @brief File information Structure.
 * 
 * Structure with file information displayed used in the user interface.
 */
typedef struct {
    bool directory;                             /**< Directory rather than a file */
    time_t mtime;                               /**< Last modification time */
    uint64_t size;                              /**< File size in bytes */
    fat_file_attributes_t fat_file_attributes;  /**< FAT file attributes */
    pak_file_attributes_t pak_file_attributes;  /**< Additional attributes for pak files */
} file_info_t;

/**
 * @brief Draw a box component.
 * 
 * @param x0 Starting x-coordinate.
 * @param y0 Starting y-coordinate.
 * @param x1 Ending x-coordinate.
 * @param y1 Ending y-coordinate.
 * @param color Color of the box.
 */
void ui_components_box_draw(int x0, int y0, int x1, int y1, color_t color);

/**
 * @brief Attach a display surface and clear it to the palette's background.
 */
void ui_components_attach_clear(surface_t *d);

/**
 * @brief Draw text that may be truncated with an ellipsis (safe replacement for rdpq_text_printn).
 *
 * @param parms Text parameters.
 * @param font Font ID.
 * @param x Left edge.
 * @param y Baseline (or top edge if parms->height is set).
 * @param text UTF-8 text.
 */
void ui_components_text_draw(const rdpq_textparms_t *parms, menu_font_type_t font, int x, int y, const char *text);

/**
 * @brief Draw body text: the 16px body font with its shadow (1px right and down) behind it.
 *
 * @param parms Layout and style (colour) of the text; the shadow uses the same layout.
 */
void ui_components_body_text_draw(const rdpq_textparms_t *parms, int x, int y, const char *text);

/**
 * @brief Draw a screen's name in the top bar, optionally followed by "  >  subtitle" in grey
 *        (e.g. "Config  >  1080 Snowboarding"), cut short with "..." at the right edge.
 */
void ui_components_screen_title_draw(const char *name, const char *subtitle);

/**
 * @brief Draw body text with a chosen shadow style (e.g. STL_SHADOW_DARK on grey surfaces).
 */
void ui_components_body_text_draw_shadowed(const rdpq_textparms_t *parms, int x, int y, const char *text, menu_font_style_t shadow_style);

/** @brief printf-style ui_components_body_text_draw(). */
void ui_components_body_text_printf(const rdpq_textparms_t *parms, int x, int y, const char *fmt, ...);

/**
 * @brief Full loading screen: header message, the game's title and a progress bar (attaches and shows d).
 *
 * @param d Display surface.
 * @param progress 0.0 to 1.0.
 * @param message Header text, e.g. "Loading".
 * @param file_name File name of the game or disk; shown as a cleaned-up title.
 */
void ui_components_loading_screen_draw(surface_t *d, float progress, const char *message, const char *file_name);

/**
 * @brief Draw a border component.
 * 
 * @param x0 Starting x-coordinate.
 * @param y0 Starting y-coordinate.
 * @param x1 Ending x-coordinate.
 * @param y1 Ending y-coordinate.
 */
void ui_components_border_draw(int x0, int y0, int x1, int y1);

/**
 * @brief Draw the layout component with tabs.
 */
void ui_components_layout_draw_tabbed(void);

/**
 * @brief Draw the layout component.
 */
void ui_components_layout_draw(void);

/**
 * @brief Draw a progress bar component.
 * 
 * @param x0 Starting x-coordinate.
 * @param y0 Starting y-coordinate.
 * @param x1 Ending x-coordinate.
 * @param y1 Ending y-coordinate.
 * @param progress Progress value (0.0 to 1.0).
 */
void ui_components_progressbar_draw(int x0, int y0, int x1, int y1, float progress);

/**
 * @brief Draw a seek bar component.
 * 
 * @param progress Progress value (0.0 to 1.0).
 */
void ui_components_seekbar_draw(float progress);

/**
 * @brief Draw a loader component.
 * 
 * @param position Position value (0.0 to 1.0).
 * @param msg Message to display, truncated to 30 characters.
 */
void ui_components_loader_draw(float progress, const char *msg);

/**
 * @brief Draw a scrollbar component.
 * 
 * @param x Starting x-coordinate.
 * @param y Starting y-coordinate.
 * @param width Width of the scrollbar.
 * @param height Height of the scrollbar.
 * @param position Current position.
 * @param items Total number of items.
 * @param visible_items Number of visible items.
 */
void ui_components_scrollbar_draw(int x, int y, int width, int height, int position, int items, int visible_items);

/**
 * @brief Draw a list scrollbar component.
 * 
 * @param position Current position.
 * @param items Total number of items.
 * @param visible_items Number of visible items.
 */
void ui_components_list_scrollbar_draw(int position, int items, int visible_items);

/**
 * @brief Draw a dialog component.
 * 
 * @param width Width of the dialog.
 * @param height Height of the dialog.
 */
void ui_components_dialog_draw(int width, int height);

/**
 * @brief Draw a message box component.
 * 
 * @param fmt Format string for the message.
 * @param ... Additional arguments for the format string.
 */
void ui_components_messagebox_draw(char *fmt, ...);

/**
 * @brief Draw the main text component.
 * 
 * @param style The font style.
 * @param align Horizontal alignment.
 * @param valign Vertical alignment.
 * @param fmt Format string for the text.
 * @param ... Additional arguments for the format string.
 */
void ui_components_main_text_draw(menu_font_type_t style, rdpq_align_t align, rdpq_valign_t valign, char *fmt, ...);

/**
 * @brief Draw the actions bar text component.
 * 
 * @param style The font style.
 * @param align Horizontal alignment.
 * @param valign Vertical alignment.
 * @param fmt Format string for the text.
 * @param ... Additional arguments for the format string.
 */
void ui_components_actions_bar_text_draw(menu_font_type_t style, rdpq_align_t align, rdpq_valign_t valign, char *fmt, ...);

/**
 * @brief Initialize the background component.
 * 
 * @param cache_location Location of the cache.
 */
void ui_components_background_init(char *cache_location);

/**
 * @brief Free the background component resources.
 */
void ui_components_background_free(void);

/**
 * @brief Free and delete the cached background image.
 */
void ui_components_background_clear(void);

/**
 * @brief Replace the background image.
 * 
 * @param image New background image.
 */
void ui_components_background_replace_image(surface_t *image);

/**
 * @brief Draw the background component.
 */
void ui_components_background_draw(void);

/**
 * @brief Return the current background image surface, or NULL if none is set.
 */
surface_t *ui_components_background_get_image(void);

/**
 * @brief Reload the background image from cache (call after temporarily freeing).
 */
void ui_components_background_reload(void);

/**
 * @brief Free only the in-memory image and display list, keeping the cache intact.
 */
void ui_components_background_image_free_only(void);

/**
 * @brief Draw the file list component.
 * 
 * @param list List of entries.
 * @param entries Number of entries.
 * @param selected Index of the selected entry.
 */
void ui_components_file_list_draw(entry_t *list, int entries, int selected);

/**
 * @brief Free file list component cached resources.
 */
void ui_components_file_list_free(void);

/**
 * @brief Draw the Library carousel row and the selected entry's title.
 *
 * @param directory Directory the entries belong to, or NULL if each entry's name is a full path.
 * @param list List of entries.
 * @param entries Number of entries.
 * @param selected Index of the selected entry.
 * @param selected_favorite The selected entry is a favorite (gold selection outline).
 */
void ui_components_carousel_draw(path_t *directory, entry_t *list, int32_t entries, int32_t selected, bool selected_favorite);

/** @brief Load the game loading animation's frames into memory (before loading: rom:/ is overwritten). */
void ui_components_loading_animation_prepare(void);
/** @brief Free the game loading animation's frames. */
void ui_components_loading_animation_free(void);
/** @brief Draw the game loading animation for a progress of 0-1 (bottom-right quadrant). */
void ui_components_loading_animation_draw(float progress);
/** @brief How far the animation's opening fade-in is, 0-1. */
float ui_components_loading_animation_fade_in(void);
/** @brief Loading finished: play the corona. */
void ui_components_loading_animation_done(void);
/** @brief Whether the corona has finished playing after ui_components_loading_animation_done(). */
bool ui_components_loading_animation_finished(void);

/**
 * @brief Scroll bar for a table whose rows are title-style text (option lists, Cheat Codes):
 *        from the top of the first visible row to the bottom of the last. Draws nothing if all
 *        rows fit.
 *
 * @param first_row_y Baseline of the first visible row.
 * @param pitch Distance between rows.
 * @param first_visible Index of the first visible row.
 * @param count Number of rows.
 * @param visible Number of rows that fit.
 */
void ui_components_table_scrollbar_draw(int first_row_y, int pitch, int first_visible, int count, int visible);

/** @brief Forget any held direction (call when a carousel view opens). */
void ui_components_carousel_scroll_reset(void);
/** @brief Handle ←/→ for a circular carousel list; a long hold pages by letter if letter_paging. Returns the new selection. */
int32_t ui_components_carousel_scroll(menu_t *menu, entry_t *list, int32_t count, int32_t selected, bool letter_paging);
/** @brief Draw the letter being paged to (top left), fading in and out. */
void ui_components_letter_indicator_draw(void);
/** @brief Draw the position counter (index over total, top right), fading like the letter indicator; index 0 draws nothing. */
void ui_components_position_indicator_draw(int index, int total);

/** @brief Controller button icons (assets/images/button_*.png, see scripts/make_icons.py). */
typedef enum {
    ICON_A,
    ICON_B,
    ICON_C_RIGHT,
    ICON_C_UP,
    ICON_C_LEFT,
    ICON_C_DOWN,
    ICON_Z,
    ICON_START,
    ICON_L,         /**< Left end of the tab bar (L pill curving into the bar) */
    ICON_R,         /**< Right end of the tab bar (mirror of ICON_L) */
    ICON_COUNT,
} ui_icon_t;

/**
 * @brief Draw a button icon with its top-left corner at (x, y).
 */
void ui_components_icon_draw(ui_icon_t icon, int x, int y);

/**
 * @brief Width of a button icon (pixels).
 */
int ui_components_icon_width(ui_icon_t icon);

/**
 * @brief Draw a button icon followed by a label ("(A) Play Cartridge").
 *
 * @param icon Button icon.
 * @param x Left edge.
 * @param baseline Baseline of the label (FNT_DEFAULT).
 * @param text Label.
 * @return Horizontal space used.
 */
int ui_components_button_hint_draw(ui_icon_t icon, int x, int baseline, const char *text);

/** @brief A hint for two buttons with the same action: "(icon) ▪ (second) Label", separated by a small square. */
int ui_components_button_hint_draw_stacked(ui_icon_t icon, ui_icon_t behind, int x, int baseline, const char *text);

/** @brief One button hint in a hint row. */
typedef struct {
    ui_icon_t icon;
    const char *text;
    ui_icon_t behind;       /**< With stacked: a second button that does the same, shown as "(icon) ▪ (behind)" */
    bool stacked;
} button_hint_t;

/** @brief Show or hide every button hint row (Menu Settings > Controller Hints). */
void ui_components_button_hints_enable(bool enabled);

/** @brief Most hints a hint row can hold. */
#define BUTTON_HINTS_MAX    (6)

/**
 * @brief Draw a row of button hints centred at the bottom of the screen.
 *
 * Pass them in the order A, B, C-Left, C-Up, C-Right, C-Down (then anything else).
 */
void ui_components_button_hints_draw(const button_hint_t *hints, int count);

/** @brief Top-level tabs, switched with L / R. */
typedef enum {
    TAB_LIBRARY,
    TAB_FAVORITES,
    TAB_HISTORY,
    TAB_SETTINGS,
    TAB_COUNT,
} menu_tab_t;

/**
 * @brief Draw the tab strip (current tab white, others gray) and the L / R icons.
 */
void ui_components_tab_header_draw(menu_tab_t current);

/**
 * @brief Handle L / R (previous / next tab) and Start (Settings tab).
 *
 * @return true if a tab switch was requested (menu->next_mode is set).
 */
bool ui_components_tab_process(menu_t *menu, menu_tab_t current);

/**
 * @brief Forget cached labels; call whenever the directory listing changes.
 */
void ui_components_carousel_invalidate(void);

/**
 * @brief Draw one page of the Library info panel for an entry.
 *
 * Page 0 is the overview (players, accessories, region, credits, dates), page 1 the
 * boot details, page 2 the description.
 *
 * @param directory Directory containing the entry, or NULL if the entry's name is a full path.
 * @param entry Selected entry (folders draw nothing).
 * @param bookkeeping History, used for the last played date.
 * @param page Page to draw (0 .. ui_components_game_info_page_count() - 1).
 */
void ui_components_game_info_draw(path_t *directory, entry_t *entry, bookkeeping_t *bookkeeping, int page);

/**
 * @brief Number of info pages available for an entry (0 for folders).
 */
int ui_components_game_info_page_count(entry_t *entry);

/**
 * @brief Scroll the About page's description by a line (-1 up, +1 down).
 *
 * @return true if it scrolled; false when the About page isn't showing or the text is at that end.
 */
bool ui_components_game_info_scroll(int direction);

/**
 * @brief Draw the info page indicator dots.
 *
 * @param page Current page.
 * @param count Number of pages (nothing is drawn for fewer than 2).
 */
void ui_components_game_info_dots_draw(int page, int count);

/**
 * @brief Forget the cached info for the selected entry.
 */
void ui_components_game_info_invalidate(void);

/** @brief Turn the screenshot gallery (Menu Settings > Screenshot Gallery) on or off. */
void ui_components_game_info_screenshots_enable(bool enabled);

/**
 * @brief Turn a file name into a display title (drops extension and region tags, fixes ", The").
 *
 * @param name File or directory name.
 * @param directory True if name is a directory (keeps any dots).
 * @param out Output buffer.
 * @param out_size Size of the output buffer.
 */
void ui_components_carousel_title(const char *name, bool directory, char *out, size_t out_size);

/**
 * @brief Title the carousel shows for an entry: its metadata title if loaded, else from the file name.
 *
 * @param entry The entry.
 * @param position Its position in the list passed to ui_components_carousel_draw().
 * @param out Output buffer.
 * @param out_size Size of the output buffer.
 */
void ui_components_carousel_entry_title(entry_t *entry, int32_t position, char *out, size_t out_size);

/**
 * @brief Context menu structure.
 */
typedef struct component_context_menu {
    int row_count; /**< Number of rows in the context menu */
    int row_selected; /**< Index of the selected row */
    bool hide_pending; /**< Flag to indicate if hiding is pending */
    struct component_context_menu *parent; /**< Pointer to the parent context menu */
    struct component_context_menu *submenu; /**< Pointer to the submenu */
    int (*get_default_selection)(menu_t *menu); /**< Optional function to get the default selected row */
    /**
     * @brief Optional: something drawn after each row's text (e.g. palette colour squares),
     *        extra_width pixels wide. The rows are then left-aligned so it lines up in a column.
     */
    void (*draw_extra)(int row, int x, int y_centre, bool selected);
    int extra_width;
    struct {
        const char *text; /**< Text of the menu item */
        void (*action)(menu_t *menu, void *arg); /**< Action function for the menu item */
        void *arg; /**< Argument for the action function */
        struct component_context_menu *submenu; /**< Pointer to the submenu */
    } list[]; /**< List of menu items */
} component_context_menu_t;

#define COMPONENT_CONTEXT_MENU_LIST_END { .text = NULL } /**< End marker for the context menu list */

/** @brief Kind of row in an option list. */
typedef enum {
    OPTION_TOGGLE,  /**< On / Off, flipped in place with A */
    OPTION_CHOICE,  /**< One of several values, picked from a pop-up with A */
    OPTION_ACTION,  /**< Opens another screen or runs something with A (no action: read-only row) */
    OPTION_INFO,    /**< Read-only label and value (value in white) */
} option_type_t;

/** @brief One row of an option list. */
typedef struct {
    const char *label;                          /**< Row name */
    const char *description;                    /**< One-line help shown below the list (optional) */
    option_type_t type;                         /**< Kind of row */
    bool (*get)(menu_t *menu);                  /**< OPTION_TOGGLE: current value */
    void (*set)(menu_t *menu, bool value);      /**< OPTION_TOGGLE: change the value */
    component_context_menu_t *picker;           /**< OPTION_CHOICE: pop-up whose current row is the value */
    void (*action)(menu_t *menu);               /**< OPTION_ACTION: what A does */
    const char *(*value)(menu_t *menu);         /**< Optional value text (overrides the picker's row text) */
    const char *action_name;                    /**< OPTION_ACTION: A's hint (default "Open") */
} option_t;

/** @brief A scrollable list of options. */
typedef struct {
    option_t *options;      /**< Rows */
    int count;              /**< Number of rows */
    int selected;           /**< Selected row */
    int first_visible;      /**< First row on screen */
} option_list_t;

/**
 * @brief Reset the selection and prepare the pickers; call from the view's init.
 */
void ui_components_option_list_init(option_list_t *list);

/**
 * @brief Handle up/down/A for the list and its open picker.
 *
 * @return true if the input was used.
 */
bool ui_components_option_list_process(menu_t *menu, option_list_t *list);

/**
 * @brief What A does on the selected row ("Toggle", "Change" or "Open"), for the button hints.
 */
const char *ui_components_option_list_action_name(option_list_t *list);

/**
 * @brief Draw rows from y_top to y_bottom (baselines), the selected row's description, and the open picker.
 */
void ui_components_option_list_draw(menu_t *menu, option_list_t *list, int y_top, int y_bottom);

/**
 * @brief Draw a whole option-list screen: header title, rows, and the A / B button hints.
 */
void ui_components_option_screen_draw(menu_t *menu, const char *title, option_list_t *list);

/** @brief What happened to the on-screen keyboard this frame. */
typedef enum {
    KEYBOARD_EDITING,   /**< Still open */
    KEYBOARD_DONE,      /**< Closed with DONE / Start: use ui_components_keyboard_text() */
    KEYBOARD_CANCELLED, /**< Closed with Z: discard */
} keyboard_result_t;

/**
 * @brief Open the on-screen QWERTY keyboard dialog.
 *
 * @param title Dialog title.
 * @param initial Starting text (may be NULL).
 * @param max_length Maximum number of characters.
 */
void ui_components_keyboard_open(const char *title, const char *initial, int max_length);

/** @brief Whether the keyboard is showing (give it all input while it is). */
bool ui_components_keyboard_is_open(void);

/** @brief Handle input for the open keyboard. */
keyboard_result_t ui_components_keyboard_process(menu_t *menu);

/** @brief The text typed (valid after KEYBOARD_DONE). */
const char *ui_components_keyboard_text(void);

/** @brief Draw the keyboard dialog and its button hints (nothing when closed). */
void ui_components_keyboard_draw(void);

/** @brief What the palette editor did this frame. */
typedef enum {
    PALETTE_EDITOR_EDITING,     /**< Still open */
    PALETTE_EDITOR_DONE,        /**< Saved: use ui_components_palette_editor_colours() */
    PALETTE_EDITOR_CANCELLED,   /**< Closed without saving */
} palette_editor_result_t;

/** @brief Open the palette editor on a custom palette, starting from its colours. */
void ui_components_palette_editor_open(ui_palette_id_t id);
/** @brief Whether the palette editor is showing (the view should hand it all input). */
bool ui_components_palette_editor_is_open(void);
/** @brief Handle input for the open palette editor. */
palette_editor_result_t ui_components_palette_editor_process(menu_t *menu);
/** @brief The palette being edited. */
ui_palette_id_t ui_components_palette_editor_palette(void);
/** @brief The edited colours (valid after PALETTE_EDITOR_DONE). */
void ui_components_palette_editor_colours(color_t out[UI_PALETTE_COLOURS]);
/** @brief Draw the palette editor and its button hints (nothing when closed). */
void ui_components_palette_editor_draw(void);

/**
 * @brief Initialize the context menu component.
 * 
 * @param cm Pointer to the context menu structure.
 */
void ui_components_context_menu_init(component_context_menu_t *cm);

/**
 * @brief Show the context menu component.
 * 
 * @param cm Pointer to the context menu structure.
 */
void ui_components_context_menu_show(component_context_menu_t *cm);

/**
 * @brief Process the context menu component.
 * 
 * @param menu Pointer to the menu structure.
 * @param cm Pointer to the context menu structure.
 * @return True if the context menu was processed, false otherwise.
 */
bool ui_components_context_menu_process(menu_t *menu, component_context_menu_t *cm);

/**
 * @brief Draw the context menu component.
 * 
 * @param cm Pointer to the context menu structure.
 */
void ui_components_context_menu_draw(component_context_menu_t *cm);

/**
 * @brief Box Art Structure.
 */
typedef struct {
    bool loading; /**< Flag to indicate if the box art is loading */
    surface_t *image; /**< Pointer to the box art image */
} component_boxart_t;

/**
 * @brief Initialize the box art component.
 * 
 * @param storage_prefix Prefix for the storage location.
 * @param game_code Game code for the box art.
 * @param rom_title Title of the ROM (may be NULL). If used, it is sanitized for filesystem safety.
 * @param current_image_view Current image view type.
 * @return Pointer to the initialized box art component.
 */
component_boxart_t *ui_components_boxart_init(const char *storage_prefix, const char *game_code, const char *rom_title, file_image_type_t current_image_view);

/**
 * @brief Free the box art component resources.
 * 
 * @param b Pointer to the box art component.
 */
void ui_components_boxart_free(component_boxart_t *b);

/**
 * @brief Draw the box art component.
 * 
 * @param b Pointer to the box art component.
 */
void ui_components_boxart_draw(component_boxart_t *b);

/**
 * @brief Draw the tabs component.
 * 
 * @param text Array of tab labels.
 * @param count Number of tabs.
 * @param selected Index of the selected tab.
 * @param width Width of the tabs.
 */
void ui_components_tabs_draw(const char **text, int count, int selected, float width);

/**
 * @brief Draw the common part of the tabs component.
 * 
 * @param selected Index of the selected tab.
 */
void ui_components_tabs_common_draw(int selected);

/**
 * @brief Draw a value editor component.
 * 
 * @param header_text Array of header text for the values.
 * @param value_text Array of value text to be displayed.
 * @param count Number of values.
 * @param selected Index of the selected value.
 * @param width_adjustment Negative width adjustment of each value box.
 */
void ui_component_value_editor(const char **header_text, const char **value_text, int count, int selected, float width_adjustment);

/**
 * @brief Draw the file info component.
 * 
 * @param filename Name of the file for which to show the information.
 * @param info Metadata information of the file to be displayed.
 */
void ui_components_file_info_draw (char* filename, file_info_t *info);

#endif /* UI_COMPONENTS_H__ */
