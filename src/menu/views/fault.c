#include "views.h"


static void draw (menu_t *menu, surface_t *d) {
    rdpq_attach(d, NULL);

    rdpq_clear(RGBA32(0x00, 0x00, 0x00, 0xFF));

    const char *firmware_message = (
        "Minimum supported firmware version:\n"
        "SummerCart64: 2.17.0+"
    );

    ui_components_messagebox_draw(
        "UNRECOVERABLE ERROR\n"
        "\n"
        "%s\n"
        "\n"
        "%s",
        flashcart_convert_error_message(menu->flashcart_err),
        (menu->flashcart_err == FLASHCART_ERR_OUTDATED) ? firmware_message : ""
    );

    rdpq_detach_show();
}


void view_fault_init (menu_t *menu) {
    // Nothing to initialize (yet)
}

void view_fault_display (menu_t *menu, surface_t *display) {
    draw(menu, display);
}
