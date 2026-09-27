/**
 * @file loading_animation.c
 * @brief Game loading animation: the boot animation's eclipse, drawn live and driven by progress
 * @ingroup ui_components
 *
 * The eclipse from the boot animation (boot_animation/make_eclipse_intro.py), without the title:
 * the sun and moon fade in, the moon's approach along its shallow arc follows the loading
 * progress (the sun turning from dark orange to white and the moon from grey to black as it
 * covers it), and once loading is done the corona appears, holds, then fades out so the game
 * starts from black.
 *
 * It is drawn from two soft masks (scripts/make_eclipse_masks.py: a disc for the sun and moon, a
 * ring for the corona), tinted and alpha-blended, so its edges suit any palette's background; the
 * pre-rendered boot frames are smoothed against black and left dark fringes on other colours.
 *
 * On the SummerCart64 the game is loaded over the cartridge space the menu's own files (rom:/)
 * live in, so the masks are loaded by ui_components_loading_animation_prepare() before loading.
 */

#include <math.h>

#include "../ui_components.h"
#include "constants.h"
#include "utils/utils.h"

#define FADE_IN_MS          (300)   // sun and moon fade in, moon at its start
#define CORONA_MS           (100)   // once loaded: the corona fades in,
#define DONE_HOLD_MS        (350)   // holds,
#define RING_FADE_MS        (550)   // then fades out (100% loaded is still totality).
                                    // Corona + hold + fade is 1s: the length of the loading wind.

// Geometry and colours from boot_animation/make_eclipse_intro.py.
#define RADIUS              (28.0f)             // sun and moon
#define START_GAP           (2.0f)              // between the discs at the start
#define APPROACH_X          (-190.0f)           // direction the moon starts from (lower left)
#define APPROACH_Y          (40.0f)
#define ARC_BOW             (4.0f)              // the approach bows this far off a straight line
#define DISC_MASK_CENTRE    (29)                // eclipse_disc.png: centre pixel
#define RING_MASK_CENTRE    (33)                // eclipse_ring.png: centre pixel
static const float SUN_START[3] = { 184, 86, 26 }, SUN_END[3] = { 255, 255, 255 };
static const float MOON_START[3] = { 112, 112, 112 }, MOON_END[3] = { 0, 0, 0 };

static sprite_t *disc;
static sprite_t *ring;
static bool started;
static uint32_t start_ms;
static bool done;
static uint32_t done_ms;


/**
 * @brief Load the masks into memory. Call before loading starts (rom:/ is overwritten after).
 */
void ui_components_loading_animation_prepare (void) {
    if (!disc) {
        disc = sprite_load("rom:/eclipse_disc.sprite");
        ring = sprite_load("rom:/eclipse_ring.sprite");
    }
    started = false;
    done = false;
}

/**
 * @brief Free the masks.
 */
void ui_components_loading_animation_free (void) {
    rspq_wait();    // the RDP may still be drawing them
    if (disc) {
        sprite_free(disc);
        sprite_free(ring);
        disc = NULL;
        ring = NULL;
    }
    started = false;
    done = false;
}

/**
 * @brief How far the opening fade-in is, 0-1 (for things that fade in with the animation).
 */
float ui_components_loading_animation_fade_in (void) {
    if (!started) {
        return 0.0f;
    }
    return MIN(1.0f, (get_ticks_ms() - start_ms) / (float) FADE_IN_MS);
}

/**
 * @brief Mark loading as finished: the corona plays next.
 */
void ui_components_loading_animation_done (void) {
    if (!done) {
        done = true;
        done_ms = get_ticks_ms();
    }
}

/**
 * @brief Whether the corona has finished after ui_components_loading_animation_done().
 */
bool ui_components_loading_animation_finished (void) {
    return done && (get_ticks_ms() - done_ms) >= CORONA_MS + DONE_HOLD_MS + RING_FADE_MS;
}

/** @brief The moon's centre relative to the sun's for an approach of t (0 start, 1 centred): uniform motion on a circular arc. */
static void moon_offset (float t, float *x, float *y) {
    float distance = (2.0f * RADIUS) + START_GAP;
    float length = sqrtf((APPROACH_X * APPROACH_X) + (APPROACH_Y * APPROACH_Y));
    float start_x = APPROACH_X * distance / length;
    float start_y = APPROACH_Y * distance / length;
    float arc_radius = (distance * distance) / (8.0f * ARC_BOW) + (ARC_BOW / 2.0f);
    float arc_angle = 2.0f * asinf(distance / (2.0f * arc_radius));

    float ux = -start_x / distance, uy = -start_y / distance;   // towards the sun
    float nx = uy, ny = -ux;                                     // bows upward
    float mid_x = start_x / 2.0f, mid_y = start_y / 2.0f;
    float angle = (t - 0.5f) * arc_angle;
    float along = arc_radius * sinf(angle);
    float bow = arc_radius * (cosf(angle) - cosf(arc_angle / 2.0f));
    *x = mid_x + (ux * along) + (nx * bow);
    *y = mid_y + (uy * along) + (ny * bow);
}

static float srgb_to_linear (float v) {
    v /= 255.0f;
    return (v <= 0.04045f) ? v / 12.92f : powf((v + 0.055f) / 1.055f, 2.4f);
}

static uint8_t linear_to_srgb (float v) {
    v = (v <= 0.0031308f) ? 12.92f * v : 1.055f * powf(v, 1.0f / 2.4f) - 0.055f;
    return (uint8_t) MAX(0.0f, MIN(255.0f, (v * 255.0f) + 0.5f));
}

/** @brief Blend two colours in linear light, as the boot animation does. */
static color_t colour_lerp (const float *from, const float *to, float p, uint8_t alpha) {
    uint8_t c[3];
    for (int i = 0; i < 3; i++) {
        c[i] = linear_to_srgb((srgb_to_linear(from[i]) * (1.0f - p)) + (srgb_to_linear(to[i]) * p));
    }
    return RGBA32(c[0], c[1], c[2], alpha);
}

/** @brief Draw a mask tinted with colour (its alpha scales the mask's), centred on (x, y). */
static void draw_mask (sprite_t *mask, int mask_centre, float x, float y, color_t colour) {
    rdpq_set_prim_color(colour);
    rdpq_sprite_blit(mask, roundf(x) - mask_centre, roundf(y) - mask_centre, NULL);
}

/**
 * @brief Draw the animation for the given loading progress (0-1), over whatever is on screen.
 */
void ui_components_loading_animation_draw (float progress) {
    if (!disc) {
        return;
    }
    uint32_t now = get_ticks_ms();
    if (!started) {
        started = true;
        start_ms = now;
    }

    // Before the fade-in ends the moon waits at its start; then it follows the progress.
    float fade = ui_components_loading_animation_fade_in();
    float t = (fade < 1.0f) ? 0.0f : MAX(0.0f, MIN(1.0f, progress));
    if (done) {
        t = 1.0f;
    }
    float moon_x, moon_y;
    moon_offset(t, &moon_x, &moon_y);

    // Colours follow how far the moon covers the sun (smoothstep of its distance).
    float p = 1.0f - (sqrtf((moon_x * moon_x) + (moon_y * moon_y)) / (2.0f * RADIUS));
    p = MAX(0.0f, MIN(1.0f, p));
    p = p * p * (3.0f - (2.0f * p));
    uint8_t alpha = (uint8_t) (fade * 0xFF);

    float sun_x = LOADING_ANIMATION_CENTER_X;
    float sun_y = LOADING_ANIMATION_CENTER_Y;

    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER1((TEX0, 0, PRIM, 0), (TEX0, 0, PRIM, 0)));
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);

        // At totality the moon covers the sun exactly; leave the sun out so its soft edge
        // doesn't show around the moon's.
        if (t < 1.0f) {
            draw_mask(disc, DISC_MASK_CENTRE, sun_x, sun_y, colour_lerp(SUN_START, SUN_END, p, alpha));
        }
        draw_mask(disc, DISC_MASK_CENTRE, sun_x + moon_x, sun_y + moon_y, colour_lerp(MOON_START, MOON_END, p, alpha));

        if (done) {
            uint32_t since = now - done_ms;
            float corona = 1.0f;
            if (since < CORONA_MS) {
                corona = since / (float) CORONA_MS;
            } else if (since > CORONA_MS + DONE_HOLD_MS) {
                corona = 1.0f - MIN(1.0f, (since - CORONA_MS - DONE_HOLD_MS) / (float) RING_FADE_MS);
            }
            draw_mask(ring, RING_MASK_CENTRE, sun_x, sun_y, RGBA32(0xFF, 0xFF, 0xFF, (uint8_t) (corona * 0xFF)));
        }
    rdpq_mode_pop();
}
