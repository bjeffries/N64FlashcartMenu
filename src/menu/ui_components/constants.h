/**
 * @file constants.h
 * @brief UI component layout and color constants for the menu system.
 * @ingroup ui_components
 *
 * This header defines all layout, sizing, and color constants used by UI components.
 * Colors are UI roles; their values come from the palette (palette.h), except those that have
 * to match the button icons and cartridges (sprite_colors.h).
 */

#ifndef COMPONENTS_CONSTANTS_H__
#define COMPONENTS_CONSTANTS_H__

#include "palette.h"
#include "sprite_colors.h"

/**
 * @def TAB_HEIGHT
 * @brief Height of the tabs in the main menu (pixels).
 */
#define TAB_HEIGHT                      (20)

/**
 * @def BORDER_THICKNESS
 * @brief Thickness of UI borders (pixels).
 */
#define BORDER_THICKNESS                (4)

/**
 * @def DISPLAY_WIDTH
 * @brief Width of the display (pixels).
 */
#define DISPLAY_WIDTH                   (640)
/**
 * @def DISPLAY_HEIGHT
 * @brief Height of the display (pixels).
 */
#define DISPLAY_HEIGHT                  (480)

/**
 * @def DISPLAY_CENTER_X
 * @brief Center X coordinate of the display.
 */
#define DISPLAY_CENTER_X                (DISPLAY_WIDTH / 2)
/**
 * @def DISPLAY_CENTER_Y
 * @brief Center Y coordinate of the display.
 */
#define DISPLAY_CENTER_Y                (DISPLAY_HEIGHT / 2)

/**
 * @def OVERSCAN_WIDTH
 * @brief Overscan margin on the X axis (pixels).
 */
#define OVERSCAN_WIDTH                  (32)
/**
 * @def OVERSCAN_HEIGHT
 * @brief Overscan margin on the Y axis (pixels).
 */
#define OVERSCAN_HEIGHT                 (24)

/**
 * @def VISIBLE_AREA_X0
 * @brief Start X coordinate of the visible display area.
 */
#define VISIBLE_AREA_X0                 (OVERSCAN_WIDTH)
/**
 * @def VISIBLE_AREA_Y0
 * @brief Start Y coordinate of the visible display area.
 */
#define VISIBLE_AREA_Y0                 (OVERSCAN_HEIGHT)
/**
 * @def VISIBLE_AREA_X1
 * @brief End X coordinate of the visible display area.
 */
#define VISIBLE_AREA_X1                 (DISPLAY_WIDTH - OVERSCAN_WIDTH)
/**
 * @def VISIBLE_AREA_Y1
 * @brief End Y coordinate of the visible display area.
 */
#define VISIBLE_AREA_Y1                 (DISPLAY_HEIGHT - OVERSCAN_HEIGHT)

/**
 * @def VISIBLE_AREA_WIDTH
 * @brief Width of the visible display area (pixels).
 */
#define VISIBLE_AREA_WIDTH              (VISIBLE_AREA_X1 - VISIBLE_AREA_X0)
/**
 * @def VISIBLE_AREA_HEIGHT
 * @brief Height of the visible display area (pixels).
 */
#define VISIBLE_AREA_HEIGHT             (VISIBLE_AREA_Y1 - VISIBLE_AREA_Y0)

/**
 * @def LAYOUT_ACTIONS_SEPARATOR_Y
 * @brief Y coordinate for the layout actions separator.
 */
#define LAYOUT_ACTIONS_SEPARATOR_Y      (400)

/**
 * @def SEEKBAR_HEIGHT
 * @brief Height of the seek bar (pixels).
 */
#define SEEKBAR_HEIGHT                  (24)
/**
 * @def SEEKBAR_WIDTH
 * @brief Width of the seek bar (pixels).
 */
#define SEEKBAR_WIDTH                   (524)
/**
 * @def SEEKBAR_X
 * @brief X coordinate of the seek bar.
 */
#define SEEKBAR_X                       (DISPLAY_CENTER_X - (SEEKBAR_WIDTH / 2))
/**
 * @def SEEKBAR_Y
 * @brief Y coordinate of the seek bar.
 */
#define SEEKBAR_Y                       (VISIBLE_AREA_Y1 - SEEKBAR_HEIGHT - 80)

/**
 * @def LOADER_WIDTH
 * @brief Width of the loader bar (pixels).
 */
#define LOADER_WIDTH                    (320)
/**
 * @def LOADER_HEIGHT
 * @brief Height of the loader bar (pixels).
 */
#define LOADER_HEIGHT                   (6)
/**
 * @def LOADER_X
 * @brief X coordinate of the loader bar.
 */
#define LOADER_X                        (DISPLAY_CENTER_X - (LOADER_WIDTH / 2))
/**
 * @def LOADER_Y
 * @brief Y coordinate of the loader bar (accounts for message height).
 */
#define LOADER_Y                        (DISPLAY_CENTER_Y - (LOADER_HEIGHT / 2) - 8)

/**
 * @def MESSAGEBOX_MAX_WIDTH
 * @brief Maximum width of a message box (pixels).
 */
#define MESSAGEBOX_MAX_WIDTH            (360)
/**
 * @def MESSAGEBOX_MARGIN
 * @brief Margin around a message box (pixels).
 */
#define MESSAGEBOX_MARGIN               (32)

/**
 * @def TEXT_MARGIN_HORIZONTAL
 * @brief Horizontal margin for text (pixels).
 */
#define TEXT_MARGIN_HORIZONTAL          (10)
/**
 * @def TEXT_MARGIN_VERTICAL
 * @brief Vertical margin for text (pixels).
 */
#define TEXT_MARGIN_VERTICAL            (6)
/**
 * @def TEXT_OFFSET_VERTICAL
 * @brief Vertical offset for text (pixels).
 */
#define TEXT_OFFSET_VERTICAL            (1)
/**
 * @def TEXT_LINE_SPACING_ADJUST
 * @brief Adjustment for text line spacing (pixels).
 */
#define TEXT_LINE_SPACING_ADJUST        (-6) // Cancels Analogue OS line gap: 18px line pitch at 20px

/**
 * @def BOXART_WIDTH
 * @brief Width of the boxart image (pixels).
 */
#define BOXART_WIDTH                    (158)
/**
 * @def BOXART_HEIGHT
 * @brief Height of the boxart image (pixels).
 */
#define BOXART_HEIGHT                   (112)

/**
 * @def BOXART_WIDTH_DD
 * @brief Width of the 64DD boxart image (pixels).
 */
#define BOXART_WIDTH_DD                 (129)
/**
 * @def BOXART_HEIGHT_DD
 * @brief Height of the 64DD boxart image (pixels).
 */
#define BOXART_HEIGHT_DD                (112)

/**
 * @def BOXART_WIDTH_MAX
 * @brief Maximum width of the boxart image (pixels).
 */
#define BOXART_WIDTH_MAX                (158)
/**
 * @def BOXART_HEIGHT_MAX
 * @brief Maximum height of the boxart image (pixels).
 */
#define BOXART_HEIGHT_MAX               (158)

/**
 * @def BOXART_X
 * @brief X coordinate for boxart image.
 */
#define BOXART_X                        (VISIBLE_AREA_X1 - BOXART_WIDTH - 24)
/**
 * @def BOXART_Y
 * @brief Y coordinate for boxart image.
 */
#define BOXART_Y                        (LAYOUT_ACTIONS_SEPARATOR_Y - BOXART_HEIGHT - 24)
/**
 * @def BOXART_X_JP
 * @brief X coordinate for Japanese boxart image.
 */
#define BOXART_X_JP                     (VISIBLE_AREA_X1 - BOXART_WIDTH_MAX + 21)
/**
 * @def BOXART_Y_JP
 * @brief Y coordinate for Japanese boxart image.
 */
#define BOXART_Y_JP                     (LAYOUT_ACTIONS_SEPARATOR_Y - BOXART_HEIGHT_MAX - 24)

/**
 * @def BOXART_X_DD
 * @brief X coordinate for 64DD boxart image.
 */
#define BOXART_X_DD                     (VISIBLE_AREA_X1 - BOXART_WIDTH_DD - 23)
/**
 * @def BOXART_Y_DD
 * @brief Y coordinate for 64DD boxart image.
 */
#define BOXART_Y_DD                     (LAYOUT_ACTIONS_SEPARATOR_Y - BOXART_HEIGHT_DD - 24)

/**
 * @def LIST_SCROLLBAR_WIDTH
 * @brief Width of the list scrollbar (pixels).
 */
#define LIST_SCROLLBAR_WIDTH            (12)
/**
 * @def LIST_SCROLLBAR_HEIGHT
 * @brief Height of the list scrollbar (pixels).
 */
#define LIST_SCROLLBAR_HEIGHT           (LAYOUT_ACTIONS_SEPARATOR_Y - OVERSCAN_HEIGHT - TAB_HEIGHT - BORDER_THICKNESS)
/**
 * @def LIST_SCROLLBAR_X
 * @brief X coordinate of the list scrollbar.
 */
#define LIST_SCROLLBAR_X                (VISIBLE_AREA_X1 - LIST_SCROLLBAR_WIDTH)
/**
 * @def LIST_SCROLLBAR_Y
 * @brief Y coordinate of the list scrollbar.
 */
#define LIST_SCROLLBAR_Y                (VISIBLE_AREA_Y0 + TAB_HEIGHT + BORDER_THICKNESS)

/**
 * @def LIST_ENTRIES
 * @brief Maximum number of file list entries.
 */
#define LIST_ENTRIES                    (19)
/**
 * @def FILE_LIST_MAX_WIDTH
 * @brief Maximum width for a file list entry (pixels).
 */
#define FILE_LIST_MAX_WIDTH             (480)
/**
 * @def FILE_LIST_HIGHLIGHT_WIDTH
 * @brief Width of the file list highlight (pixels).
 */
#define FILE_LIST_HIGHLIGHT_WIDTH       (VISIBLE_AREA_X1 - VISIBLE_AREA_X0 - LIST_SCROLLBAR_WIDTH)
/**
 * @def FILE_LIST_HIGHLIGHT_X
 * @brief X coordinate of the file list highlight.
 */
#define FILE_LIST_HIGHLIGHT_X           (VISIBLE_AREA_X0)

/**
 * @def BACKGROUND_EMPTY_COLOR
 * @brief Color used when no background image is present (RGBA8888).
 */
#define BACKGROUND_EMPTY_COLOR          PALETTE_BACKGROUND
/**
 * @def BACKGROUND_OVERLAY_COLOR
 * @brief Overlay color for the background (RGBA8888, semi-transparent).
 */
#define BACKGROUND_OVERLAY_COLOR        PALETTE_WITH_ALPHA(PALETTE_BACKGROUND, 0xA0)

/**
 * @def BORDER_COLOR
 * @brief Color of UI borders (RGBA8888).
 */
#define BORDER_COLOR                    PALETTE_HIGHLIGHT

/**
 * @def PROGRESSBAR_BG_COLOR
 * @brief Background color of the progress bar (RGBA8888).
 */
#define PROGRESSBAR_BG_COLOR            PALETTE_TONE_2
/**
 * @def PROGRESSBAR_DONE_COLOR
 * @brief Color of the completed portion of the progress bar (RGBA8888).
 */
#define PROGRESSBAR_DONE_COLOR          PALETTE_HIGHLIGHT

/**
 * @def SCROLLBAR_BG_COLOR
 * @brief Background color of the scrollbar (RGBA8888).
 */
#define SCROLLBAR_BG_COLOR              PALETTE_TONE_2
/**
 * @def SCROLLBAR_INACTIVE_COLOR
 * @brief Inactive color of the scrollbar (RGBA8888).
 */
#define SCROLLBAR_INACTIVE_COLOR        PALETTE_TONE_3
/**
 * @def SCROLLBAR_POSITION_COLOR
 * @brief Color of the scrollbar position indicator (RGBA8888).
 */
#define SCROLLBAR_POSITION_COLOR        PALETTE_TONE_3

/**
 * @def DIALOG_BG_COLOR
 * @brief Background color for dialog boxes (RGBA8888).
 */
#define DIALOG_BG_COLOR                 PALETTE_TONE_1
/** @brief Dialog outline color and thickness (matches the selection outline). */
#define DIALOG_BORDER_COLOR             PALETTE_HIGHLIGHT
#define DIALOG_BORDER                   (2)
/** @brief Thin line above the button hints on non-Library screens. */
#define LAYOUT_SEPARATOR_COLOR          PALETTE_TONE_2

/**
 * @def BOXART_LOADING_COLOR
 * @brief Color used while boxart is loading (RGBA8888).
 */
#define BOXART_LOADING_COLOR            PALETTE_BACKGROUND

/**
 * @def FILE_LIST_HIGHLIGHT_COLOR
 * @brief Highlight color for file list entries (RGBA8888).
 */
#define FILE_LIST_HIGHLIGHT_COLOR       PALETTE_TONE_3

/**
 * @def CONTEXT_MENU_HIGHLIGHT_COLOR
 * @brief Highlight color for context menu entries (RGBA8888).
 */
#define CONTEXT_MENU_HIGHLIGHT_COLOR    PALETTE_HIGHLIGHT

/**
 * @def TAB_INACTIVE_BORDER_COLOR
 * @brief Border color for inactive tabs (RGBA8888).
 */
#define TAB_INACTIVE_BORDER_COLOR       PALETTE_TONE_3
/**
 * @def TAB_ACTIVE_BORDER_COLOR
 * @brief Border color for active tabs (RGBA8888).
 */
#define TAB_ACTIVE_BORDER_COLOR         PALETTE_HIGHLIGHT
/**
 * @def TAB_INACTIVE_BACKGROUND_COLOR
 * @brief Background color for inactive tabs (RGBA8888).
 */
#define TAB_INACTIVE_BACKGROUND_COLOR   PALETTE_TONE_2
/**
 * @def TAB_ACTIVE_BACKGROUND_COLOR
 * @brief Background color for active tabs (RGBA8888).
 */
#define TAB_ACTIVE_BACKGROUND_COLOR     PALETTE_TONE_3

/*
 * Library (carousel) layout. Text Y values are baselines.
 * The selected tile, header, title and info labels share one left edge;
 * info values and button hints share a second one (GAME_INFO_VALUE_X).
 */

/** @brief Left edge of the selected tile, header, title and info labels. */
#define CAROUSEL_SELECTED_X             (VISIBLE_AREA_X0 + 8)
/** @brief Baseline of the screen header ("Library"). */
#define LIBRARY_HEADER_Y                (42)
/** @brief Top of the selected tile. */
#define CAROUSEL_TILE_Y                 (60)
/** @brief Selected tile width and height (~30% larger than the others). */
#define CAROUSEL_SELECTED_TILE_SIZE     (146)
/** @brief Unselected tile width and height. */
#define CAROUSEL_TILE_SIZE              (112)
/** @brief Top of the unselected tiles, vertically centred on the selected one. */
#define CAROUSEL_SMALL_TILE_Y           (CAROUSEL_TILE_Y + ((CAROUSEL_SELECTED_TILE_SIZE - CAROUSEL_TILE_SIZE) / 2))
/** @brief Horizontal distance between tile left edges. */
#define CAROUSEL_TILE_PITCH             (CAROUSEL_TILE_SIZE + 10)
/** @brief Space on each side of the selected tile. */
#define CAROUSEL_SELECTED_GAP           (28)

/** @brief Size of the cartridge sprites (assets/images/cartridge*.png, see scripts/make_cartridge.py). */
#define CARTRIDGE_WIDTH                 (88)
#define CARTRIDGE_HEIGHT                (62)
/** @brief Position and size of the transparent label window in the cartridge sprite. */
#define CARTRIDGE_LABEL_X               (22)
#define CARTRIDGE_LABEL_Y               (6)
#define CARTRIDGE_LABEL_WIDTH           (44)
#define CARTRIDGE_LABEL_HEIGHT          (51)
/**
 * @brief Large cartridge on the selected tile: sprite size, then label window position and size.
 * It fills the tile's width; the window plus CARTRIDGE_LABEL_BLEED on each side is exactly the
 * 74x86 source label, so the selected label is drawn unscaled.
 */
#define CARTRIDGE_LARGE_WIDTH           (146)
#define CARTRIDGE_LARGE_HEIGHT          (103)
#define CARTRIDGE_LARGE_LABEL_X         (38)
#define CARTRIDGE_LARGE_LABEL_Y         (10)
#define CARTRIDGE_LARGE_LABEL_WIDTH     (70)
#define CARTRIDGE_LARGE_LABEL_HEIGHT    (82)
/** @brief The selection outline sprite (cartridge_large_outline.png) extends this far past the large cartridge. */
#define CARTRIDGE_OUTLINE_OFFSET        (4)
/** @brief How far the label extends under the cartridge on each side, hiding the label's own edge. */
#define CARTRIDGE_LABEL_BLEED           (2)
/** @brief Baseline of the captions under unselected tiles: level with the bottom of the selected cartridge's outline. */
#define CAROUSEL_CAPTION_Y              (CAROUSEL_TILE_Y + ((CAROUSEL_SELECTED_TILE_SIZE - CARTRIDGE_LARGE_HEIGHT) / 2) + CARTRIDGE_LARGE_HEIGHT + CARTRIDGE_OUTLINE_OFFSET)
/** @brief Baseline of the selected entry's title: 22px below the captions, plus the title's 18px capitals. */
#define CAROUSEL_TITLE_Y                (CAROUSEL_CAPTION_Y + 40)
/** @brief Left edge of the info panel's labels (PLAYER COUNT, REGION, ...). */
#define GAME_INFO_LABEL_X               (CAROUSEL_SELECTED_X)
/** @brief Left edge of the info panel's values: the label column is 112px wide. */
#define GAME_INFO_VALUE_X               (GAME_INFO_LABEL_X + 112)
/**
 * @brief Overview page: game screenshot (4:3), centred on the right half of the screen, its top
 *        8px above the top of the first info row's capitals (PLAYER COUNT). Values stop short of it.
 */
#define GAME_INFO_SCREENSHOT_WIDTH      (224)
#define GAME_INFO_SCREENSHOT_HEIGHT     (168)
#define GAME_INFO_SCREENSHOT_X          (((DISPLAY_WIDTH * 3) / 4) - (GAME_INFO_SCREENSHOT_WIDTH / 2))
#define GAME_INFO_SCREENSHOT_Y          (GAME_INFO_Y - fonts_cap_height(GAME_INFO_FONT) - 8)
#define GAME_INFO_SCREENSHOT_GAP        (12)
/** @brief Left edge of the selected entry's title: indented to leave room for the page dots. */
#define GAME_INFO_TITLE_X               (GAME_INFO_LABEL_X + 14)
/** @brief Baseline of the first info row. */
#define GAME_INFO_Y                     (CAROUSEL_TITLE_Y + 30)
/**
 * @brief The UI's two text styles: 32px titles (TITLE_FONT, also the top and bottom bars) and
 *        16px body text drawn with a shadow (BODY_FONT, ui_components_body_text_draw()).
 */
#define TITLE_FONT                      FNT_TITLE
#define BODY_FONT                       FNT_DEFAULT
/** @brief Font of the info panel (and the captions under unselected games). */
#define GAME_INFO_FONT                  BODY_FONT
/** @brief Distance between info rows in a group: the info font's ascent + 3px (at least 16). */
#define GAME_INFO_ROW_PITCH             (MAX(16, fonts_ascent(GAME_INFO_FONT) + 3))
/** @brief Extra space between groups of info rows. */
#define GAME_INFO_GROUP_GAP             (6)
/** @brief Player count badge: palette tone 2 behind the player marks
 *         (one shade lighter than the read-only rows of settings screens; text uses the dark shadow). */
#define GAME_INFO_BADGE_COLOR           PALETTE_TONE_2
/** @brief Player icons: supported players, and unused player slots. */
#define GAME_INFO_PLAYER_ON_COLOR       PALETTE_HIGHLIGHT
#define GAME_INFO_PLAYER_OFF_COLOR      PALETTE_TONE_1    // darker than the badge

/** @brief About page: text line height and the first line's ascent (from the info font's ascent),
 *         and the bottom of the text area. */
#define GAME_INFO_ABOUT_LINE_HEIGHT     (MAX(14, fonts_ascent(GAME_INFO_FONT) + 3))
#define GAME_INFO_ABOUT_ASCENT          (MAX(10, fonts_ascent(GAME_INFO_FONT)))
/** @brief Info value badges: from 2px above the info font's ascent to 3px below the baseline. */
#define GAME_INFO_BADGE_TOP(y)          ((y) - fonts_ascent(GAME_INFO_FONT) - 2)
#define GAME_INFO_ABOUT_BOTTOM          (LIBRARY_BUTTONS_Y - 34)

/** @brief Size of an info page dot. */
#define GAME_INFO_DOT_SIZE              (6)
/** @brief Distance between the info page dots (three dots span the title's capitals). */
#define GAME_INFO_DOT_PITCH             (11)
/** @brief Current info page dot. */
#define GAME_INFO_DOT_ON_COLOR          PALETTE_HIGHLIGHT
/** @brief Other info page dots. */
#define GAME_INFO_DOT_OFF_COLOR         PALETTE_TONE_2

/** @brief Space between tab names in the header (widened by 1px if needed to centre exactly). */
#define TAB_HEADER_GAP                  (28)
/** @brief Space between the outer tab names and the L / R pills. */
#define TAB_BAR_TEXT_GAP                (20)
/** @brief Top of the L / R pills (tab names sit on LIBRARY_HEADER_Y). */
#define TAB_BAR_PILL_Y                  (LIBRARY_HEADER_Y - 7)
/** @brief Width of the pill part of the tab bar's end sprites (the rest curves into the bar). */
#define TAB_BAR_PILL_WIDTH              (26)
/** @brief Height of the L / R pills (scripts/make_icons.py). */
#define TAB_BAR_PILL_HEIGHT             (16)
/** @brief Thickness of the bar joining L and R; its bottom lines up with the pills'. */
#define TAB_BAR_HEIGHT                  (3)
/** @brief Tab bar colour (same gray as the L / R pills, scripts/make_icons.py). */
#define TAB_BAR_COLOR                   SPRITE_BUTTON_GRAY
/** @brief Baseline of the first Settings tab row. */
#define SETTINGS_HUB_Y                  (110)
/** @brief Distance between Settings tab rows. */
#define SETTINGS_HUB_ROW_PITCH          (36)

/** @brief Config screen: baseline of the game title and of the first option row. */
#define CONFIG_LIST_Y                   (SETTINGS_LIST_Y)
/** @brief Loading screen: baseline of the game title, and top of the progress bar. */
#define LOADING_TITLE_Y                 (220)
#define LOADING_BAR_Y                   (250)

/** @brief Baseline of the first row on option-list screens without a game title (e.g. Menu Settings). */
#define SETTINGS_LIST_Y                 (96)

/** @brief Time screen: baseline of the large date and time, and of the help text below it. */
#define TIME_VALUE_Y                    (150)
#define TIME_HELP_Y                     (240)

/** @brief On-screen keyboard: key, SHIFT-on key and text field backgrounds. */
#define KEYBOARD_KEY_COLOR              PALETTE_TONE_2
#define KEYBOARD_KEY_ACTIVE_COLOR       PALETTE_TONE_3
#define KEYBOARD_FIELD_COLOR            PALETTE_BACKGROUND

/** @brief Cheat Codes screen: left edge of the On / Off column. */
#define CHEAT_STATE_X                   (VISIBLE_AREA_X1 - 52)

/** @brief Distance between option list rows (title-style 32px text), as on the Settings tab. */
#define OPTION_LIST_ROW_PITCH           (36)
/** @brief Rows an option list shows before it scrolls, leaving room for the description below. */
#define OPTION_LIST_VISIBLE_ROWS        (8)
/** @brief Baseline of the first Cheat Codes row (its column headers sit above it). */
#define CHEAT_LIST_Y                    (110)
/** @brief Scroll bar at the right edge of scrolling tables (option lists, Cheat Codes). */
#define TABLE_SCROLLBAR_WIDTH           (4)
#define TABLE_SCROLLBAR_X               (VISIBLE_AREA_X1 - TABLE_SCROLLBAR_WIDTH)
#define TABLE_SCROLLBAR_MIN_THUMB       (8)
/** @brief Scroll bar thumb: the palette's lightest tone, halfway to white (light gray in Monochrome). */
#define TABLE_SCROLLBAR_THUMB_COLOR     palette_mix(PALETTE_TONE_3, RGBA32(0xFF, 0xFF, 0xFF, 0xFF), 0x80)
/** @brief Distance between Cheat Codes rows (title-style text, like option lists). */
#define CHEAT_ROW_PITCH                 (OPTION_LIST_ROW_PITCH)
/** @brief Background band behind read-only information rows. */
#define OPTION_LIST_INFO_BAND_COLOR     PALETTE_TONE_1
/** @brief Left edge of option values (the label column is about 23 characters wide). */
#define OPTION_LIST_VALUE_X             (310)

/** @brief Space between button hints. */
#define LIBRARY_HINT_GAP                (20)

/** @brief Baseline of the button hints at the bottom of the screen. */
#define LIBRARY_BUTTONS_Y               (VISIBLE_AREA_Y1 - 10)

/** @brief Scroll easing rate (1/s): higher is snappier. 18 covers ~90% of a step in 130ms. */
#define CAROUSEL_SCROLL_SPEED           (18.0f)
/** @brief Scroll distances beyond this many tiles jump and animate only the last step. */
#define CAROUSEL_MAX_ANIMATED_STEPS     (3.0f)
/** @brief While ←/→ is held, move one tile every this many frames (12 per second at 60fps). */
#define CAROUSEL_REPEAT_FRAMES          (5)
/**
 * @brief Game loading animation: centre of the eclipse. The info panel is blacked out while a game
 *        loads; the eclipse is centred in it: across the screen, and between the top of the first
 *        info row and the top of the "Loading" text (which sits on the button hints' baseline).
 */
#define LOADING_ANIMATION_CENTER_X      (DISPLAY_CENTER_X)
#define LOADING_ANIMATION_CENTER_Y      (((GAME_INFO_Y - 14) + (LIBRARY_BUTTONS_Y - 14)) / 2)
/** @brief Game loading: from this progress on, the screen behind the eclipse fades to black. */
#define LOADING_FADE_START              (0.8f)
/** @brief Emulators only (no SummerCart64): how long the pretend game load takes. */
#define SIMULATED_LOAD_MS               (3000)
/** @brief Holding ←/→ this long switches from tiles to letters (Library, Favorites). */
#define CAROUSEL_PAGING_DELAY_MS        (1000)
/** @brief While paging, one letter every this long. */
#define CAROUSEL_PAGING_INTERVAL_MS     (400)
/** @brief Paging letter: baseline (title font; its capitals top out at the safe area). */
#define LETTER_INDICATOR_Y              (VISIBLE_AREA_Y0 + fonts_cap_height(FNT_TITLE))
/** @brief Paging letter: left edge, lined up with the cartridge, page dots and info labels. */
#define LETTER_INDICATOR_X              (GAME_INFO_LABEL_X)
#define LETTER_INDICATOR_FADE_IN_MS     (120)
/** @brief Position counter (a vertical fraction): space above and below its bar, and the bar's thickness. */
#define POSITION_FRACTION_GAP           (3)
#define POSITION_FRACTION_BAR           (2)
/** @brief Position counter: how far the bar reaches past the wider number, and its minimum width. */
#define POSITION_FRACTION_BAR_OVERHANG  (2)
#define POSITION_FRACTION_BAR_MIN       (14)
/** @brief Paging letter stays this long after the last page, then fades out. */
#define LETTER_INDICATOR_HOLD_MS        (400)
#define LETTER_INDICATOR_FADE_OUT_MS    (300)

/** @brief Stand-in for a missing label (matches the cartridge's label recess). */
#define CAROUSEL_PLACEHOLDER_COLOR      SPRITE_CARTRIDGE_DETAIL
/** @brief Folder icon color. */
#define CAROUSEL_FOLDER_COLOR           SPRITE_CARTRIDGE_BODY

/** @brief Text (STL_DEFAULT). */
#define TEXT_COLOR                      PALETTE_HIGHLIGHT
/** @brief Secondary text: labels, unselected tabs and captions, values you can't change (STL_GRAY). */
#define TEXT_SECONDARY_COLOR            PALETTE_TONE_3
/**
 * @brief Text shadow (STL_SHADOW), drawn 1px right and down behind small text. On a dark background
 *        it has to be lighter than the background to show; it also gives 1px horizontal strokes a
 *        second line, so they appear in both interlaced fields (less flicker on a CRT).
 */
#define TEXT_SHADOW_COLOR               PALETTE_TONE_2
#define TEXT_SHADOW_OFFSET              (1)
/** @brief Shadow for text on grey surfaces lighter than the background (keyboard keys). */
#define TEXT_SHADOW_DARK_COLOR          PALETTE_BACKGROUND
/** @brief Softer text on a highlight-coloured surface (the selected keyboard key), and its shadow. */
#define TEXT_SOFT_COLOR                 PALETTE_TONE_3
#define TEXT_SHADOW_SOFT_COLOR          palette_mix(PALETTE_TONE_3, PALETTE_HIGHLIGHT, 0x80)
/** @brief Text on highlight-coloured backgrounds, e.g. the selected keyboard key (STL_BLACK). */
#define TEXT_ON_LIGHT_COLOR             PALETTE_BACKGROUND
/** @brief What the boot and game-loading animations fade to: always black, whatever the palette. */
#define FADE_COLOR                      RGBA32(0x00, 0x00, 0x00, 0xFF)
/** @brief Screen background. */
#define BACKGROUND_COLOR                PALETTE_BACKGROUND
/** @brief Outline around the selected folder (cartridges use sprites drawn in the same white). */
#define CAROUSEL_OUTLINE_COLOR          SPRITE_OUTLINE_SELECTED
/** @brief Bar marking the selected row in option lists and the Settings tab. */
#define SELECTION_MARKER_COLOR          PALETTE_HIGHLIGHT
/** @brief Underline of the field / digit being edited (Time, Cheat Codes). */
#define EDIT_UNDERLINE_COLOR            PALETTE_HIGHLIGHT
/** @brief Selected keyboard key. */
#define KEYBOARD_KEY_SELECTED_COLOR     PALETTE_HIGHLIGHT
/** @brief Keyboard text cursor. */
#define KEYBOARD_CURSOR_COLOR           PALETTE_HIGHLIGHT

#endif /* COMPONENTS_CONSTANTS_H__ */
