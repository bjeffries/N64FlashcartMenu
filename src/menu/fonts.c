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

static void load_title_font (void) {
    rdpq_font_t *title_font = rdpq_font_load("rom:/AnalogueOS-40.font64");
    register_styles(title_font);
    rdpq_text_register_font(FNT_TITLE, title_font);
}


void fonts_init (char *custom_font_path) {
    load_default_font(custom_font_path);
    load_title_font();
}
