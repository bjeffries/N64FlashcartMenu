/**
 * @file n64_logo.c
 * @brief The spinning N64 logo (scripts/make_n64_logo.py): the Library's screenshot placeholder
 *        and the screensaver.
 * @ingroup ui_components
 */

#include <libdragon.h>

#include "../ui_components.h"
#include "../n64_logo_frames.h"
#include "constants.h"
#include "utils/utils.h"

// The most recently drawn frames (the trails screensaver draws three a frame).
#define CACHED_FRAMES   (3)
static sprite_t *logo[CACHED_FRAMES];
static int logo_frame[CACHED_FRAMES] = { -1, -1, -1 };
static int oldest = 0;

// Grey version of the frames' palette (every frame shares one), for the screensaver's trail.
#define PALETTE_SIZE    (256)
static uint16_t *gray_palette = NULL;


static void free_sprite (void *sprite) {
    sprite_free(sprite);
}

/** @brief A frame's sprite; frames are loaded from rom:/ as they come up, as all of them won't fit in memory. */
static sprite_t *get_frame (int frame) {
    sprite_t *sprite = NULL;
    for (int i = 0; i < CACHED_FRAMES; i++) {
        if (logo_frame[i] == frame) {
            sprite = logo[i];
        }
    }
    if (!sprite) {
        int slot = oldest;
        oldest = (oldest + 1) % CACHED_FRAMES;
        if (logo[slot]) {
            // Freed once the RDP has drawn it, rather than waiting for that here.
            rdpq_call_deferred(free_sprite, logo[slot]);
        }
        char path[32];
        snprintf(path, sizeof(path), "rom:/n64logo/%03d.sprite", frame);
        logo[slot] = sprite_load(path);
        logo_frame[slot] = frame;
        sprite = logo[slot];
    }
    return sprite;
}

void ui_components_n64_logo_draw (int x, int y, int frame, uint8_t alpha) {
    if (alpha == 0) {
        return;
    }
    sprite_t *sprite = get_frame(frame);
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
        rdpq_sprite_blit(sprite, x, y, NULL);
    rdpq_mode_pop();
}

void ui_components_n64_logo_draw_gray (int x, int y, int frame) {
    sprite_t *sprite = get_frame(frame);
    if (!gray_palette) {
        const uint16_t *palette = sprite_get_palette(sprite);
        gray_palette = malloc_uncached_aligned(8, PALETTE_SIZE * sizeof(uint16_t));
        if (!palette || !gray_palette) {
            return;
        }
        for (int i = 0; i < PALETTE_SIZE; i++) {
            color_t c = color_from_packed16(palette[i]);
            uint8_t luma = (uint8_t) (((c.r * 77) + (c.g * 150) + (c.b * 29)) >> 8);
            gray_palette[i] = color_to_packed16(RGBA32(luma, luma, luma, c.a));
        }
    }
    surface_t pixels = sprite_get_pixels(sprite);
    rdpq_mode_push();
        rdpq_set_mode_copy(true);   // transparent background
        rdpq_mode_tlut(TLUT_RGBA16);
        rdpq_tex_upload_tlut(gray_palette, 0, PALETTE_SIZE);
        rdpq_tex_blit(&pixels, x, y, NULL);
    rdpq_mode_pop();
}

void ui_components_n64_logo_draw_many (const int *xs, const int *ys, int count, int frame) {
    if (count <= 0) {
        return;
    }
    // A logo is too big for TMEM (4KB; 2KB for CI8 texels, the palette takes the rest), so it's
    // drawn in strips of rows. Each strip is loaded once and drawn at every position before the
    // next one, and the palette is loaded once, instead of every logo loading all of them.
    sprite_t *sprite = get_frame(frame);
    surface_t pixels = sprite_get_pixels(sprite);
    int strip_rows = 2048 / pixels.stride;

    rdpq_mode_push();
        rdpq_set_mode_copy(true);   // transparent background
        rdpq_mode_tlut(TLUT_RGBA16);
        rdpq_tex_upload_tlut(sprite_get_palette(sprite), 0, PALETTE_SIZE);
        for (int t0 = 0; t0 < pixels.height; t0 += strip_rows) {
            int t1 = MIN(t0 + strip_rows, (int) pixels.height);
            bool loaded = false;
            for (int i = 0; i < count; i++) {
                int y0 = ys[i] + t0;
                int y1 = ys[i] + t1;
                if (y1 <= 0 || y0 >= DISPLAY_HEIGHT || xs[i] >= DISPLAY_WIDTH || xs[i] + (int) pixels.width <= 0) {
                    continue;   // this strip of this logo is off screen
                }
                if (!loaded) {
                    rdpq_tex_upload_sub(TILE0, &pixels, NULL, 0, t0, pixels.width, t1);
                    loaded = true;
                }
                rdpq_texture_rectangle(TILE0, xs[i], y0, xs[i] + pixels.width, y1, 0, t0);
            }
        }
    rdpq_mode_pop();
}

int ui_components_n64_logo_frame (void) {
    // Half a turn rendered at 30fps (the logo looks the same after half a turn), looped.
    return (int) ((get_ticks_ms() % N64_LOGO_LOOP_MS) * N64_LOGO_FRAMES / N64_LOGO_LOOP_MS);
}
