/**
 * @file n64_logo.c
 * @brief The spinning N64 logo (scripts/make_n64_logo.py): the Library's screenshot placeholder
 *        and the screensaver.
 * @ingroup ui_components
 */

#include <libdragon.h>

#include "../ui_components.h"
#include "../n64_logo_frames.h"

static sprite_t *logo;
static int logo_frame = -1;


void ui_components_n64_logo_draw (int x, int y, int frame) {
    // Frames are loaded from rom:/ one at a time as they come up, as all of them won't fit in memory.
    if (frame != logo_frame) {
        if (logo) {
            rspq_wait();    // the RDP may still be drawing the last frame
            sprite_free(logo);
        }
        char path[32];
        snprintf(path, sizeof(path), "rom:/n64logo/%03d.sprite", frame);
        logo = sprite_load(path);
        logo_frame = frame;
    }
    rdpq_mode_push();
        rdpq_set_mode_copy(true);   // transparent background
        rdpq_sprite_blit(logo, x, y, NULL);
    rdpq_mode_pop();
}

int ui_components_n64_logo_frame (void) {
    // Half a turn rendered at 30fps (the logo looks the same after half a turn), looped.
    return (int) ((get_ticks_ms() % N64_LOGO_LOOP_MS) * N64_LOGO_FRAMES / N64_LOGO_LOOP_MS);
}
