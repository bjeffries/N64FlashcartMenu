/**
 * @file palette_editor.c
 * @brief Palette editor dialog for the custom palettes (Menu Settings > Palette > Custom 1 / 2)
 * @ingroup ui_components
 *
 * Opens over Menu Settings like the on-screen keyboard. Three stages:
 *   - colour list: the five colours (background, tone 1-3, highlight) and Save. Up / Down choose,
 *     A edits, Z resets the colour to Monochrome's, B cancels, Start (or Save) saves;
 *   - colour grid: 64 preset colours. The D-pad moves, A chooses, C-Right fine-tunes, B goes back;
 *   - fine-tune: red, green and blue in the screen's 32 steps. Up / Down choose the channel,
 *     Left / Right change it, A is done, B goes back to the grid.
 * A small preview window shows the palette being edited (the menu itself doesn't change until
 * it is saved). Leaving with unsaved changes asks first: A leaves without saving, B keeps editing.
 */

#include <math.h>
#include <string.h>

#include "../ui_components.h"
#include "../fonts.h"
#include "../sound.h"
#include "constants.h"
#include "utils/utils.h"

#define GRID_SIZE           (8)
#define CELL                (18)
#define CELL_PITCH          (20)
#define DIALOG_WIDTH        (548)
#define DIALOG_HEIGHT       (320)
#define ROW_PITCH           (30)
#define SLOTS               (UI_PALETTE_COLOURS)
#define SAVE_ROW            (SLOTS)
#define PREVIEW_WIDTH       (200)
#define PREVIEW_HEIGHT      (150)
#define SLIDER_WIDTH        (128)

typedef enum { STAGE_LIST, STAGE_GRID, STAGE_FINE } stage_t;

static const char *slot_names[SLOTS] = { "Background", "Tone 1", "Tone 2", "Tone 3", "Highlight" };
static const uint32_t monochrome[SLOTS] = { 0x000000, 0x1E1E1E, 0x404040, 0x808080, 0xFFFFFF };

static struct {
    bool open;
    ui_palette_id_t id;
    stage_t stage;
    color_t colours[SLOTS];
    color_t original[SLOTS];    // as opened, to tell whether anything changed
    bool confirm_exit;          // "Exit without saving?" is showing
    int row;                    // colour list row (SAVE_ROW = Save)
    int grid_x, grid_y;
    int channel;                // fine-tune: 0 red, 1 green, 2 blue
    color_t before_fine;        // restored if fine-tuning is backed out of
} editor;

static color_t grid[GRID_SIZE][GRID_SIZE];
static bool grid_ready;


/** @brief Round a channel to what the 16-bit screen can show (5 bits), expanded back to 8. */
static uint8_t screen_level (float v) {
    int level = (int) (MAX(0.0f, MIN(1.0f, v)) * 31.0f + 0.5f);
    return (uint8_t) ((level << 3) | (level >> 2));
}

static color_t hsl (float h, float s, float l) {
    float c = (1.0f - fabsf((2.0f * l) - 1.0f)) * s;
    float hp = h / 60.0f;
    float x = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f));
    float r = 0, g = 0, b = 0;
    if (hp < 1) { r = c; g = x; } else if (hp < 2) { r = x; g = c; } else if (hp < 3) { g = c; b = x; }
    else if (hp < 4) { g = x; b = c; } else if (hp < 5) { r = x; b = c; } else { r = c; b = x; }
    float m = l - (c / 2.0f);
    return RGBA32(screen_level(r + m), screen_level(g + m), screen_level(b + m), 0xFF);
}

/** @brief The 64 presets: a gray ramp, then eight hues from dark (backgrounds) to light (highlights). */
static void build_grid (void) {
    static const float hues[GRID_SIZE] = { 0, 28, 52, 120, 175, 215, 270, 320 };
    static const float lightness[GRID_SIZE - 1] = { 0.12f, 0.20f, 0.30f, 0.42f, 0.56f, 0.70f, 0.84f };
    static const float saturation[GRID_SIZE - 1] = { 0.40f, 0.45f, 0.50f, 0.55f, 0.60f, 0.65f, 0.75f };
    for (int x = 0; x < GRID_SIZE; x++) {
        grid[0][x] = hsl(0, 0, x / (float) (GRID_SIZE - 1));
        for (int y = 1; y < GRID_SIZE; y++) {
            grid[y][x] = hsl(hues[x], saturation[y - 1], lightness[y - 1]);
        }
    }
    grid_ready = true;
}

static int channel_level (color_t c, int channel) {
    uint8_t v = (channel == 0) ? c.r : ((channel == 1) ? c.g : c.b);
    return v >> 3;
}

static void set_channel_level (color_t *c, int channel, int level) {
    uint8_t v = (uint8_t) ((level << 3) | (level >> 2));
    if (channel == 0) c->r = v; else if (channel == 1) c->g = v; else c->b = v;
}

/** @brief Put the grid cursor on the preset nearest to a colour. */
static void grid_select_nearest (color_t c) {
    int best = -1;
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            int dr = grid[y][x].r - c.r, dg = grid[y][x].g - c.g, db = grid[y][x].b - c.b;
            int d = (dr * dr) + (dg * dg) + (db * db);
            if (best < 0 || d < best) {
                best = d;
                editor.grid_x = x;
                editor.grid_y = y;
            }
        }
    }
}

/**
 * @brief Open the editor on a custom palette, starting from its current colours.
 */
void ui_components_palette_editor_open (ui_palette_id_t id) {
    if (!grid_ready) {
        build_grid();
    }
    editor.open = true;
    editor.id = id;
    editor.stage = STAGE_LIST;
    editor.row = 0;
    editor.channel = 0;
    editor.confirm_exit = false;
    ui_palette_colours(id, editor.colours);
    memcpy(editor.original, editor.colours, sizeof(editor.colours));
}

static bool changed (void) {
    for (int i = 0; i < SLOTS; i++) {
        if (ui_palette_to_rgb(editor.colours[i]) != ui_palette_to_rgb(editor.original[i])) {
            return true;
        }
    }
    return false;
}

/** @brief Whether the editor is showing (the view should hand it all input). */
bool ui_components_palette_editor_is_open (void) {
    return editor.open;
}

/** @brief The palette it edits, and its colours (valid after PALETTE_EDITOR_DONE). */
ui_palette_id_t ui_components_palette_editor_palette (void) {
    return editor.id;
}

void ui_components_palette_editor_colours (color_t out[UI_PALETTE_COLOURS]) {
    memcpy(out, editor.colours, sizeof(editor.colours));
}

/**
 * @brief Handle input while the editor is open.
 *
 * @return PALETTE_EDITOR_DONE or _CANCELLED when it closes, otherwise _EDITING.
 */
palette_editor_result_t ui_components_palette_editor_process (menu_t *menu) {
    if (!editor.open) {
        return PALETTE_EDITOR_CANCELLED;
    }
    // C-buttons also report directions (go_fast); here only the D-pad / stick move.
    bool up = menu->actions.go_up && !menu->actions.go_fast;
    bool down = menu->actions.go_down && !menu->actions.go_fast;
    bool left = menu->actions.go_left && !menu->actions.go_fast;
    bool right = menu->actions.go_right && !menu->actions.go_fast;

    if (editor.confirm_exit) {
        if (menu->actions.enter) {          // exit without saving
            editor.confirm_exit = false;
            editor.open = false;
            sound_play_effect(SFX_EXIT);
            return PALETTE_EDITOR_CANCELLED;
        } else if (menu->actions.back) {    // keep editing
            editor.confirm_exit = false;
            sound_play_effect(SFX_SETTING);
        }
        return PALETTE_EDITOR_EDITING;
    }

    if (menu->actions.settings) {       // Start saves from anywhere
        editor.open = false;
        sound_play_effect(SFX_ENTER);
        return PALETTE_EDITOR_DONE;
    }

    switch (editor.stage) {
        case STAGE_LIST:
            if (up || down) {
                editor.row = (editor.row + (up ? SLOTS : 1)) % (SLOTS + 1);
                sound_play_effect(SFX_CURSOR);
            } else if (menu->actions.enter) {
                if (editor.row == SAVE_ROW) {
                    editor.open = false;
                    sound_play_effect(SFX_ENTER);
                    return PALETTE_EDITOR_DONE;
                }
                grid_select_nearest(editor.colours[editor.row]);
                editor.stage = STAGE_GRID;
                sound_play_effect(SFX_ENTER);
            } else if (menu->actions.lz_context && !menu->actions.tab_prev && editor.row < SLOTS) {  // Z
                editor.colours[editor.row] = ui_palette_from_rgb(monochrome[editor.row]);
                sound_play_effect(SFX_SETTING);
            } else if (menu->actions.back) {
                if (changed()) {
                    editor.confirm_exit = true;     // ask before losing the changes
                    sound_play_effect(SFX_SETTING);
                } else {
                    editor.open = false;
                    sound_play_effect(SFX_EXIT);
                    return PALETTE_EDITOR_CANCELLED;
                }
            }
            break;

        case STAGE_GRID:
            if (up || down) {
                editor.grid_y = (editor.grid_y + (up ? GRID_SIZE - 1 : 1)) % GRID_SIZE;
                sound_play_effect(SFX_CURSOR);
            } else if (left || right) {
                editor.grid_x = (editor.grid_x + (left ? GRID_SIZE - 1 : 1)) % GRID_SIZE;
                sound_play_effect(SFX_CURSOR);
            } else if (menu->actions.enter) {
                editor.colours[editor.row] = grid[editor.grid_y][editor.grid_x];
                editor.stage = STAGE_LIST;
                sound_play_effect(SFX_SETTING);
            } else if (menu->actions.c_right) {
                editor.before_fine = editor.colours[editor.row];
                editor.colours[editor.row] = grid[editor.grid_y][editor.grid_x];
                editor.channel = 0;
                editor.stage = STAGE_FINE;
                sound_play_effect(SFX_ENTER);
            } else if (menu->actions.back) {
                editor.stage = STAGE_LIST;
                sound_play_effect(SFX_EXIT);
            }
            break;

        case STAGE_FINE: {
            color_t *c = &editor.colours[editor.row];
            if (up || down) {
                editor.channel = (editor.channel + (up ? 2 : 1)) % 3;
                sound_play_effect(SFX_CURSOR);
            } else if (left || right) {
                int level = channel_level(*c, editor.channel) + (left ? -1 : 1);
                if (level >= 0 && level <= 31) {
                    set_channel_level(c, editor.channel, level);
                    sound_play_effect(SFX_CURSOR);
                }
            } else if (menu->actions.enter) {
                editor.stage = STAGE_LIST;
                sound_play_effect(SFX_SETTING);
            } else if (menu->actions.back) {
                *c = editor.before_fine;
                editor.stage = STAGE_GRID;
                sound_play_effect(SFX_EXIT);
            }
            break;
        }
    }
    return PALETTE_EDITOR_EDITING;
}

static void draw_swatch (int x, int y, int size, color_t colour) {
    ui_components_box_draw(x, y, x + size, y + size, PALETTE_SWATCH_OUTLINE_COLOR);
    ui_components_box_draw(x + 1, y + 1, x + size - 1, y + size - 1, colour);
}

static void draw_outline (int x0, int y0, int x1, int y1, int t, color_t colour) {
    ui_components_box_draw(x0, y0, x1, y0 + t, colour);
    ui_components_box_draw(x0, y1 - t, x1, y1, colour);
    ui_components_box_draw(x0, y0, x0 + t, y1, colour);
    ui_components_box_draw(x1 - t, y0, x1, y1, colour);
}

/** @brief A small mock of the menu in the colours being edited. */
static void draw_preview (int x, int y, const color_t c[SLOTS]) {
    fonts_set_style_colour(STL_PREVIEW_TEXT, c[4]);
    fonts_set_style_colour(STL_PREVIEW_SECONDARY, c[3]);
    fonts_set_style_colour(STL_PREVIEW_SHADOW, c[2]);
    rdpq_textparms_t text = { .style_id = STL_PREVIEW_TEXT };
    rdpq_textparms_t secondary = { .style_id = STL_PREVIEW_SECONDARY };

    draw_outline(x - 1, y - 1, x + PREVIEW_WIDTH + 1, y + PREVIEW_HEIGHT + 1, 1, PALETTE_SWATCH_OUTLINE_COLOR);
    ui_components_box_draw(x, y, x + PREVIEW_WIDTH, y + PREVIEW_HEIGHT, c[0]);

    ui_components_body_text_draw_shadowed(&text, x + 12, y + 24, "Library", STL_PREVIEW_SHADOW);
    ui_components_body_text_draw_shadowed(&secondary, x + 76, y + 24, "Faves", STL_PREVIEW_SHADOW);

    // A read-only row band, and a selected row with its marker.
    ui_components_box_draw(x + 8, y + 40, x + PREVIEW_WIDTH - 8, y + 60, c[1]);
    ui_components_body_text_draw_shadowed(&secondary, x + 18, y + 55, "Read-only row", STL_PREVIEW_SHADOW);
    ui_components_box_draw(x + 8, y + 68, x + 12, y + 86, c[4]);
    ui_components_body_text_draw_shadowed(&text, x + 18, y + 82, "Selected row", STL_PREVIEW_SHADOW);

    // Keys: a plain one and the active one.
    ui_components_box_draw(x + 12, y + 100, x + 62, y + 122, c[2]);
    ui_components_box_draw(x + 70, y + 100, x + 120, y + 122, c[3]);
    ui_components_body_text_draw_shadowed(&(rdpq_textparms_t) { .style_id = STL_PREVIEW_TEXT, .width = 50, .align = ALIGN_CENTER },
        x + 12, y + 116, "Key", STL_PREVIEW_SHADOW);
    ui_components_body_text_draw_shadowed(&secondary, x + 12, y + 140, "Secondary text", STL_PREVIEW_SHADOW);
}

static void draw_grid (int x, int y) {
    for (int gy = 0; gy < GRID_SIZE; gy++) {
        for (int gx = 0; gx < GRID_SIZE; gx++) {
            int cx = x + (gx * CELL_PITCH);
            int cy = y + (gy * CELL_PITCH);
            ui_components_box_draw(cx, cy, cx + CELL, cy + CELL, grid[gy][gx]);
        }
    }
    int cx = x + (editor.grid_x * CELL_PITCH);
    int cy = y + (editor.grid_y * CELL_PITCH);
    draw_outline(cx - 3, cy - 3, cx + CELL + 3, cy + CELL + 3, 2, SPRITE_OUTLINE_SELECTED);
}

static void draw_sliders (int x, int y, color_t c) {
    static const char *names[3] = { "Red", "Green", "Blue" };
    for (int i = 0; i < 3; i++) {
        int baseline = y + 24 + (i * 44);
        bool selected = (i == editor.channel);
        int level = channel_level(c, i);
        ui_components_body_text_draw(&(rdpq_textparms_t) { .style_id = selected ? STL_DEFAULT : STL_GRAY }, x, baseline, names[i]);
        ui_components_body_text_printf(&(rdpq_textparms_t) { .style_id = selected ? STL_DEFAULT : STL_GRAY, .width = SLIDER_WIDTH + 32, .align = ALIGN_RIGHT },
            x, baseline, "%d", level);
        int bar_y = baseline + 8;
        color_t fill = RGBA32(i == 0 ? 0xFF : 0x40, i == 1 ? 0xFF : 0x40, i == 2 ? 0xFF : 0x40, 0xFF);
        ui_components_box_draw(x, bar_y, x + SLIDER_WIDTH + 32, bar_y + 6, PALETTE_TONE_2);
        ui_components_box_draw(x, bar_y, x + ((SLIDER_WIDTH + 32) * level) / 31, bar_y + 6, fill);
        if (selected) {
            int mx = x + ((SLIDER_WIDTH + 32) * level) / 31;
            ui_components_box_draw(mx - 1, bar_y - 3, mx + 2, bar_y + 9, SPRITE_OUTLINE_SELECTED);
        }
    }
    draw_swatch(x, y + 150, 24, c);
    ui_components_body_text_printf(&(rdpq_textparms_t) { .style_id = STL_GRAY }, x + 32, y + 168, "#%06lX", (unsigned long) ui_palette_to_rgb(c));
}

/**
 * @brief Draw the editor dialog and its button hints (does nothing when closed).
 */
void ui_components_palette_editor_draw (void) {
    if (!editor.open) {
        return;
    }
    ui_components_dialog_draw(DIALOG_WIDTH, DIALOG_HEIGHT);
    int x0 = DISPLAY_CENTER_X - (DIALOG_WIDTH / 2) + 16;
    int y0 = DISPLAY_CENTER_Y - (DIALOG_HEIGHT / 2);

    ui_components_body_text_draw(&(rdpq_textparms_t) { .style_id = STL_GRAY }, x0, y0 + 24, ui_palette_info(editor.id)->name);

    // What the preview shows: the grid colour under the cursor while choosing one.
    color_t shown[SLOTS];
    memcpy(shown, editor.colours, sizeof(shown));
    if (editor.stage == STAGE_GRID) {
        shown[editor.row] = grid[editor.grid_y][editor.grid_x];
    }

    // Colour list and Save.
    for (int i = 0; i <= SAVE_ROW; i++) {
        int baseline = y0 + 64 + (i * ROW_PITCH) + ((i == SAVE_ROW) ? 12 : 0);
        bool selected = (i == editor.row);
        if (selected) {
            ui_components_box_draw(x0 - 8, baseline - fonts_cap_height(BODY_FONT) - 4, x0 - 5, baseline + 4, SELECTION_MARKER_COLOR);
        }
        menu_font_style_t style = (selected && editor.stage == STAGE_LIST) || (selected && i < SLOTS) ? STL_DEFAULT : STL_GRAY;
        if (i < SLOTS) {
            draw_swatch(x0, baseline - 13, 16, shown[i]);
            ui_components_body_text_draw(&(rdpq_textparms_t) { .style_id = style }, x0 + 24, baseline, slot_names[i]);
        } else {
            ui_components_body_text_draw(&(rdpq_textparms_t) { .style_id = style }, x0 + 24, baseline, "Save");
        }
    }

    int middle_x = x0 + 134;
    int top_y = y0 + 44;
    if (editor.stage == STAGE_FINE) {
        draw_sliders(middle_x, top_y, editor.colours[editor.row]);
    } else {
        draw_grid(middle_x, top_y);
    }
    draw_preview(x0 + DIALOG_WIDTH - 32 - PREVIEW_WIDTH, top_y, shown);

    const char *help[3] = {
        "Choose a colour to change. Start saves.",
        "Pick a colour. C-Right fine-tunes it.",
        "Up / Down: channel. Left / Right: change it.",
    };
    ui_components_body_text_draw(&(rdpq_textparms_t) { .style_id = STL_GRAY }, x0, y0 + DIALOG_HEIGHT - 16, help[editor.stage]);

    if (editor.confirm_exit) {
        ui_components_messagebox_draw("Exit without saving?\n\nYour changes to %s will be lost.", ui_palette_info(editor.id)->name);
        ui_components_button_hints_draw((button_hint_t[]) { { ICON_A, "Exit" }, { ICON_B, "Keep Editing" } }, 2);
        return;
    }

    switch (editor.stage) {
        case STAGE_LIST:
            if (editor.row == SAVE_ROW) {
                ui_components_button_hints_draw((button_hint_t[]) { { ICON_A, "Save" }, { ICON_B, "Cancel" } }, 2);
            } else {
                ui_components_button_hints_draw((button_hint_t[]) {
                    { ICON_A, "Edit" }, { ICON_B, "Cancel" }, { ICON_START, "Save" }, { ICON_Z, "Reset" },
                }, 4);
            }
            break;
        case STAGE_GRID:
            ui_components_button_hints_draw((button_hint_t[]) {
                { ICON_A, "Choose" }, { ICON_B, "Back" }, { ICON_C_RIGHT, "Fine-tune" }, { ICON_START, "Save" },
            }, 4);
            break;
        case STAGE_FINE:
            ui_components_button_hints_draw((button_hint_t[]) { { ICON_A, "Done" }, { ICON_B, "Back" }, { ICON_START, "Save" } }, 3);
            break;
    }
}
