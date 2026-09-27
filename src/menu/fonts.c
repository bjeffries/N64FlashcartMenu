#include <libdragon.h>

#include "fonts.h"
#include "ui_components/constants.h"
#include "utils/fs.h"


static void register_styles (rdpq_font_t *font) {
    rdpq_font_style(font, STL_DEFAULT, &((rdpq_fontstyle_t) { .color = TEXT_COLOR }));
    rdpq_font_style(font, STL_GREEN, &((rdpq_fontstyle_t) { .color = PALETTE_LEGACY_GREEN }));
    rdpq_font_style(font, STL_BLUE, &((rdpq_fontstyle_t) { .color = PALETTE_LEGACY_BLUE }));
    rdpq_font_style(font, STL_YELLOW, &((rdpq_fontstyle_t) { .color = PALETTE_LEGACY_YELLOW }));
    rdpq_font_style(font, STL_ORANGE, &((rdpq_fontstyle_t) { .color = PALETTE_LEGACY_ORANGE }));
    rdpq_font_style(font, STL_RED, &((rdpq_fontstyle_t) { .color = PALETTE_LEGACY_RED }));
    rdpq_font_style(font, STL_GRAY, &((rdpq_fontstyle_t) { .color = TEXT_SECONDARY_COLOR }));
    rdpq_font_style(font, STL_BLACK, &((rdpq_fontstyle_t) { .color = TEXT_ON_LIGHT_COLOR }));
    rdpq_font_style(font, STL_FADE, &((rdpq_fontstyle_t) { .color = TEXT_COLOR }));
    rdpq_font_style(font, STL_SHADOW, &((rdpq_fontstyle_t) { .color = TEXT_SHADOW_COLOR }));
    rdpq_font_style(font, STL_SHADOW_DARK, &((rdpq_fontstyle_t) { .color = TEXT_SHADOW_DARK_COLOR }));
    rdpq_font_style(font, STL_SOFT, &((rdpq_fontstyle_t) { .color = TEXT_SOFT_COLOR }));
    rdpq_font_style(font, STL_SHADOW_SOFT, &((rdpq_fontstyle_t) { .color = TEXT_SHADOW_SOFT_COLOR }));
}

static void load_default_font (char *custom_font_path) {
    char *font_path = "rom:/font-default.font64";

    if (custom_font_path != NULL && strlen(custom_font_path) > 0) {
        // Only check file_exists if custom_font_path is a valid filesystem path (not rom:/)
        if (strncmp(custom_font_path, "rom:/", 5) != 0 && file_exists(custom_font_path)) {
            font_path = custom_font_path;
        }
    }

    rdpq_font_t *default_font = rdpq_font_load(font_path);

    register_styles(default_font);
    rdpq_text_register_font(FNT_DEFAULT, default_font);
}

// Height of each font's capitals (the font is chosen at build time: MENU_FONT in the Makefile).
static int cap_heights[FNT_LAST + 1];
static int ascents[FNT_LAST + 1];

static void measure_cap_height (menu_font_type_t id) {
    // Capitals: the ink of "H" (the paragraph box below uses the font's ascent, which is taller).
    rdpq_font_gmetrics_t metrics;
    const rdpq_font_t *font = rdpq_text_get_font(id);
    cap_heights[id] = rdpq_font_get_glyph_metrics(font, 'H', &metrics) ? -metrics.y0 : 0;

    int nbytes = 1;
    rdpq_paragraph_t *paragraph = rdpq_paragraph_build(&(rdpq_textparms_t) { 0 }, id, "H", &nbytes);
    ascents[id] = (int) (-paragraph->bbox.y0 + 0.5f);
    rdpq_paragraph_free(paragraph);
    if (cap_heights[id] <= 0) {
        cap_heights[id] = ascents[id];
    }
}

static void load_font (menu_font_type_t id, const char *path) {
    rdpq_font_t *font = rdpq_font_load(path);
    register_styles(font);
    rdpq_text_register_font(id, font);
}


void fonts_apply_palette (void) {
    for (int id = FNT_DEFAULT; id <= FNT_LAST; id++) {
        register_styles((rdpq_font_t *) rdpq_text_get_font(id));
    }
}

void fonts_set_fade_level (uint8_t font_id, uint8_t level) {
    // From the background (0) to the text colour (255).
    color_t from = BACKGROUND_COLOR;
    color_t to = TEXT_COLOR;
    color_t color = RGBA32(
        from.r + (((to.r - from.r) * level) / 0xFF),
        from.g + (((to.g - from.g) * level) / 0xFF),
        from.b + (((to.b - from.b) * level) / 0xFF),
        0xFF
    );
    rdpq_font_style((rdpq_font_t *) rdpq_text_get_font(font_id), STL_FADE, &((rdpq_fontstyle_t) { .color = color }));
}

void fonts_init (char *custom_font_path) {
    load_default_font(custom_font_path);
    load_font(FNT_TITLE, "rom:/font-title.font64");
    for (int id = FNT_DEFAULT; id <= FNT_LAST; id++) {
        measure_cap_height(id);
    }
}

int fonts_cap_height (menu_font_type_t id) {
    return cap_heights[id];
}

int fonts_ascent (menu_font_type_t id) {
    return ascents[id];
}
