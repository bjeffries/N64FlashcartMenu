#include <libdragon.h>

#include "fonts.h"
#include "utils/fs.h"


static void register_styles (rdpq_font_t *font) {
    rdpq_font_style(font, STL_DEFAULT, &((rdpq_fontstyle_t) { .color = RGBA32(0xFF, 0xFF, 0xFF, 0xFF) }));
    rdpq_font_style(font, STL_GREEN, &((rdpq_fontstyle_t) { .color = RGBA32(0x70, 0xFF, 0x70, 0xFF) }));
    rdpq_font_style(font, STL_BLUE, &((rdpq_fontstyle_t) { .color = RGBA32(0x70, 0xBC, 0xFF, 0xFF) }));
    rdpq_font_style(font, STL_YELLOW, &((rdpq_fontstyle_t) { .color = RGBA32(0xFF, 0xFF, 0x70, 0xFF) }));
    rdpq_font_style(font, STL_ORANGE, &((rdpq_fontstyle_t) { .color = RGBA32(0xFF, 0x99, 0x00, 0xFF) }));
    rdpq_font_style(font, STL_RED, &((rdpq_fontstyle_t) { .color = RGBA32(0xFF, 0x40, 0x40, 0xFF) }));
    rdpq_font_style(font, STL_GRAY, &((rdpq_fontstyle_t) { .color = RGBA32(0x80, 0x80, 0x80, 0xFF) }));
    rdpq_font_style(font, STL_BLACK, &((rdpq_fontstyle_t) { .color = RGBA32(0x00, 0x00, 0x00, 0xFF) }));
    rdpq_font_style(font, STL_FADE, &((rdpq_fontstyle_t) { .color = RGBA32(0xFF, 0xFF, 0xFF, 0xFF) }));
}

static void load_default_font (char *custom_font_path) {
    char *font_path = "rom:/AnalogueOS-20.font64";

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

static void load_font (menu_font_type_t id, const char *path) {
    rdpq_font_t *font = rdpq_font_load(path);
    register_styles(font);
    rdpq_text_register_font(id, font);
}


void fonts_set_fade_level (uint8_t font_id, uint8_t level) {
    rdpq_font_style((rdpq_font_t *) rdpq_text_get_font(font_id), STL_FADE,
        &((rdpq_fontstyle_t) { .color = RGBA32(level, level, level, 0xFF) }));
}

void fonts_init (char *custom_font_path) {
    load_default_font(custom_font_path);
    load_font(FNT_TITLE, "rom:/AnalogueOS-40.font64");
    load_font(FNT_SMALL, "rom:/AnalogueOS-12.font64");
}
