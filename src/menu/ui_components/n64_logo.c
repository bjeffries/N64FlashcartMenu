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


static void free_sprite (void *sprite) {
    sprite_free(sprite);
}

void ui_components_n64_logo_draw (int x, int y, int frame, uint8_t alpha) {
    if (alpha == 0) {
        return;
    }
    // Frames are loaded from rom:/ one at a time as they come up, as all of them won't fit in memory.
    if (frame != logo_frame) {
        if (logo) {
            // Freed once the RDP has drawn it, rather than waiting for that here.
            rdpq_call_deferred(free_sprite, logo);
        }
        char path[32];
        snprintf(path, sizeof(path), "rom:/n64logo/%03d.sprite", frame);
        logo = sprite_load(path);
        logo_frame = frame;
    }
    rdpq_mode_push();
        if (alpha == 0xFF) {
            rdpq_set_mode_copy(true);   // transparent background
        } else {
            // Fading: the frame's colours at `alpha`, its transparent pixels left out.
            rdpq_set_mode_standard();
            rdpq_mode_alphacompare(1);
            rdpq_mode_combiner(RDPQ_COMBINER1((0, 0, 0, TEX0), (TEX0, 0, PRIM, 0)));
            rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
            rdpq_set_prim_color(RGBA32(0xFF, 0xFF, 0xFF, alpha));
        }
        rdpq_sprite_blit(logo, x, y, NULL);
    rdpq_mode_pop();
}

int ui_components_n64_logo_frame (void) {
    // Half a turn rendered at 30fps (the logo looks the same after half a turn), looped.
    return (int) ((get_ticks_ms() % N64_LOGO_LOOP_MS) * N64_LOGO_FRAMES / N64_LOGO_LOOP_MS);
}
