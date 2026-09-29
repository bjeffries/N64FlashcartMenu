/**
 * @file palette.c
 * @brief The UI's colour palettes (see palette.h)
 * @ingroup ui_components
 */

#include <string.h>

#include "palette.h"

static ui_palette_t palettes[UI_PALETTE_COUNT] = {
    [UI_PALETTE_MONOCHROME] = {
        .name = "Monochrome", .key = "monochrome",
        .background = { 0x00, 0x00, 0x00, 0xFF },
        .tone_1 = { 0x1E, 0x1E, 0x1E, 0xFF },
        .tone_2 = { 0x40, 0x40, 0x40, 0xFF },
        .tone_3 = { 0x80, 0x80, 0x80, 0xFF },
        .highlight = { 0xFF, 0xFF, 0xFF, 0xFF },
    },
    // Dark purple with bold yellow.
    [UI_PALETTE_GALAXY] = {
        .name = "Galaxy", .key = "galaxy",
        .background = { 0x1A, 0x0B, 0x2E, 0xFF },
        .tone_1 = { 0x2A, 0x17, 0x47, 0xFF },
        .tone_2 = { 0x43, 0x28, 0x6B, 0xFF },
        .tone_3 = { 0x8A, 0x76, 0xA8, 0xFF },
        .highlight = { 0xFF, 0xD2, 0x3F, 0xFF },
    },
    // Muted purple with white.
    [UI_PALETTE_DUSK] = {
        .name = "Dusk", .key = "dusk",
        .background = { 0x23, 0x1A, 0x2E, 0xFF },
        .tone_1 = { 0x2F, 0x25, 0x40, 0xFF },
        .tone_2 = { 0x4A, 0x3D, 0x5E, 0xFF },
        .tone_3 = { 0x9A, 0x8F, 0xAB, 0xFF },
        .highlight = { 0xFF, 0xFF, 0xFF, 0xFF },
    },
    // Black with muted orange.
    [UI_PALETTE_DAWN] = {
        .name = "Dawn", .key = "dawn",
        .background = { 0x00, 0x00, 0x00, 0xFF },
        .tone_1 = { 0x1E, 0x1A, 0x17, 0xFF },
        .tone_2 = { 0x3D, 0x34, 0x2C, 0xFF },
        .tone_3 = { 0x8C, 0x7B, 0x6B, 0xFF },
        .highlight = { 0xE0, 0xA4, 0x6B, 0xFF },
    },
    // Edited in the palette editor; they start as Monochrome (settings load the saved colours).
    [UI_PALETTE_CUSTOM_1] = {
        .name = "Custom 1", .key = "custom1",
        .background = { 0x00, 0x00, 0x00, 0xFF },
        .tone_1 = { 0x1E, 0x1E, 0x1E, 0xFF },
        .tone_2 = { 0x40, 0x40, 0x40, 0xFF },
        .tone_3 = { 0x80, 0x80, 0x80, 0xFF },
        .highlight = { 0xFF, 0xFF, 0xFF, 0xFF },
    },
    [UI_PALETTE_CUSTOM_2] = {
        .name = "Custom 2", .key = "custom2",
        .background = { 0x00, 0x00, 0x00, 0xFF },
        .tone_1 = { 0x1E, 0x1E, 0x1E, 0xFF },
        .tone_2 = { 0x40, 0x40, 0x40, 0xFF },
        .tone_3 = { 0x80, 0x80, 0x80, 0xFF },
        .highlight = { 0xFF, 0xFF, 0xFF, 0xFF },
    },
};

static ui_palette_id_t current = UI_PALETTE_DEFAULT;
const ui_palette_t *ui_palette = &palettes[UI_PALETTE_DEFAULT];


void ui_palette_set (ui_palette_id_t id) {
    current = (id < UI_PALETTE_COUNT) ? id : UI_PALETTE_DEFAULT;
    ui_palette = &palettes[current];
}

ui_palette_id_t ui_palette_get (void) {
    return current;
}

const ui_palette_t *ui_palette_info (ui_palette_id_t id) {
    return &palettes[(id < UI_PALETTE_COUNT) ? id : UI_PALETTE_DEFAULT];
}

ui_palette_id_t ui_palette_from_key (const char *key) {
    for (int i = 0; key && i < UI_PALETTE_COUNT; i++) {
        if (strcmp(key, palettes[i].key) == 0) {
            return (ui_palette_id_t) i;
        }
    }
    return UI_PALETTE_DEFAULT;
}

void ui_palette_colours (ui_palette_id_t id, color_t out[UI_PALETTE_COLOURS]) {
    const ui_palette_t *p = ui_palette_info(id);
    out[0] = p->background;
    out[1] = p->tone_1;
    out[2] = p->tone_2;
    out[3] = p->tone_3;
    out[4] = p->highlight;
}

void ui_palette_set_colours (ui_palette_id_t id, const color_t colours[UI_PALETTE_COLOURS]) {
    if (!ui_palette_is_custom(id)) {
        return;
    }
    ui_palette_t *p = &palettes[id];
    p->background = colours[0];
    p->tone_1 = colours[1];
    p->tone_2 = colours[2];
    p->tone_3 = colours[3];
    p->highlight = colours[4];
}
