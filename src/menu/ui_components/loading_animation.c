/**
 * @file loading_animation.c
 * @brief Game loading animation: the boot animation's eclipse, drawn natively and driven by
 *        loading progress
 * @ingroup ui_components
 *
 * Plays the boot animation's eclipse (without the title): the sun and moon fade in, the moon's
 * approach follows the loading progress, and the corona appears once loading is done, holds, then
 * drops off the bottom of the screen, accelerating like something let go of, so the game starts
 * from black rather than cutting away from the ring. The eclipse
 * is centred in the Library's info panel, which is left blank while a game loads.
 *
 * The shapes are drawn with the RDP each frame rather than from pre-rendered images: every
 * circle is drawn row by row as one solid span plus antialiased edge pixels, whose alpha is the
 * pixel's coverage. Geometry, colours and timing follow boot_animation/make_eclipse_intro.py, but
 * the moon moves continuously with the loading progress, every colour is blended against the
 * actual colour behind the eclipse (the palette background, fading to black near the end), and
 * nothing has to be loaded into memory before loading starts. The look is set by the style
 * below, so it can be changed or tied to the palette in one place.
 */

#include <math.h>

#include "../ui_components.h"
#include "constants.h"
#include "utils/utils.h"

// Timing, as on the boot animation (30 fps frames).
#define FRAME_MS            (1000 / 30)
#define FADE_IN_MS          (8 * FRAME_MS)      // frames 1-9: sun and moon fade in
#define CORONA_MS           (3 * FRAME_MS)      // frames 55-57: the corona fades in
#define DONE_HOLD_MS        (350)   // show the full corona this long,
#define DROP_MS             (550)   // then drop it off the bottom of the screen (constant
                                    // acceleration from rest). With the corona that's 1s: the
                                    // length of the loading wind.
#define BLUR_SHUTTER_MS     (FRAME_MS)  // motion blur while dropping: where the ring was over this long,
#define BLUR_SPACING_PX     (3.0f)      // drawn as a fading copy every this many pixels,
#define BLUR_MAX_COPIES     (6)         // at most this many

// Geometry in pixels, relative to the sun's centre (make_eclipse_intro.py).
#define SUN_RADIUS          (28.0f)
#define MOON_RADIUS         (28.0f)
#define MOON_START_GAP      (2.0f)              // edge gap between sun and moon at the start
#define MOON_APPROACH_X     (-190.0f)           // direction the moon starts in, from the sun
#define MOON_APPROACH_Y     (40.0f)
#define MOON_ARC_BOW        (4.0f)              // the approach bows this far off a straight line
#define CORONA_INNER_RADIUS (29.0f)
#define CORONA_OUTER_RADIUS (32.0f)

/** @brief The eclipse's colours. The moon ends as the colour behind it, so it disappears into any palette. */
static const struct {
    color_t sun_start, sun_end;     // the sun brightens as the moon covers it
    color_t moon_start;             // then goes towards the background
    color_t corona;
} style = {
    .sun_start = { 0xB8, 0x56, 0x1A, 0xFF },
    .sun_end = { 0xFF, 0xFF, 0xFF, 0xFF },
    .moon_start = { 0x70, 0x70, 0x70, 0xFF },
    .corona = { 0xFF, 0xFF, 0xFF, 0xFF },
};

static bool started;
static uint32_t start_ms;
static bool done;
static uint32_t done_ms;


static float clamp01 (float value) {
    return (value < 0.0f) ? 0.0f : ((value > 1.0f) ? 1.0f : value);
}

static float srgb_to_linear (uint8_t value) {
    float v = value / 255.0f;
    return (v <= 0.04045f) ? (v / 12.92f) : powf((v + 0.055f) / 1.055f, 2.4f);
}

static uint8_t linear_to_srgb (float value) {
    float v = (value <= 0.0031308f) ? (12.92f * value) : (1.055f * powf(value, 1.0f / 2.4f) - 0.055f);
    return (uint8_t) (clamp01(v) * 255.0f + 0.5f);
}

/** @brief Blend in linear light, as the boot animation's colour transitions do. */
static color_t colour_lerp (color_t a, color_t b, float amount) {
    return RGBA32(
        linear_to_srgb(srgb_to_linear(a.r) + (srgb_to_linear(b.r) - srgb_to_linear(a.r)) * amount),
        linear_to_srgb(srgb_to_linear(a.g) + (srgb_to_linear(b.g) - srgb_to_linear(a.g)) * amount),
        linear_to_srgb(srgb_to_linear(a.b) + (srgb_to_linear(b.b) - srgb_to_linear(a.b)) * amount),
        0xFF
    );
}

/** @brief Blend in sRGB, as the boot animation's opacity fades do. */
static color_t mix (color_t a, color_t b, float amount) {
    return RGBA32(
        (uint8_t) (a.r + (b.r - a.r) * amount + 0.5f),
        (uint8_t) (a.g + (b.g - a.g) * amount + 0.5f),
        (uint8_t) (a.b + (b.b - a.b) * amount + 0.5f),
        0xFF
    );
}

/** @brief Draw one pixel-wide span in a colour at a given coverage (0-1). */
static void span (int x0, int x1, int y, color_t colour, float coverage) {
    if (x1 < x0 || coverage <= 0.0f) {
        return;
    }
    colour.a = (uint8_t) (clamp01(coverage) * 255.0f + 0.5f);
    rdpq_set_prim_color(colour);
    rdpq_fill_rectangle(x0, y, x1 + 1, y + 1);
}

/**
 * @brief Draw an antialiased ring (or disc, with inner < 0) centred on (cx, cy).
 *
 * A pixel's coverage is approximated from the distance of its centre to the edge, which is
 * accurate for circles this size: fully inside by half a pixel is solid, and so on.
 */
static void draw_ring (float cx, float cy, float inner, float outer, color_t colour) {
    int y0 = MAX(0, (int) floorf(cy - outer - 0.5f));
    int y1 = MIN(DISPLAY_HEIGHT - 1, (int) ceilf(cy + outer + 0.5f));
    for (int y = y0; y <= y1; y++) {
        float dy = (y + 0.5f) - cy;
        float reach = outer + 0.5f;
        if (fabsf(dy) >= reach) {
            continue;
        }
        float half = sqrtf(reach * reach - dy * dy);
        int x0 = (int) ceilf(cx - half - 0.5f);
        int x1 = (int) floorf(cx + half - 0.5f);

        // Runs of fully covered pixels are drawn as one span; the rest one pixel at a time.
        int run_start = -1;
        for (int x = x0; x <= x1; x++) {
            float dx = (x + 0.5f) - cx;
            float d = sqrtf(dx * dx + dy * dy);
            float coverage = clamp01(outer + 0.5f - d);
            if (inner >= 0.0f) {
                coverage = fminf(coverage, clamp01(d - inner + 0.5f));
            }
            if (coverage >= 1.0f) {
                if (run_start < 0) {
                    run_start = x;
                }
                continue;
            }
            if (run_start >= 0) {
                span(run_start, x - 1, y, colour, 1.0f);
                run_start = -1;
            }
            span(x, x, y, colour, coverage);
        }
        if (run_start >= 0) {
            span(run_start, x1, y, colour, 1.0f);
        }
    }
}

/** @brief How far the eclipse has fallen this long into the drop: from rest, accelerating so the ring's top edge clears the screen at DROP_MS. */
static float drop_offset (float ms) {
    float t = clamp01(ms / DROP_MS);
    float distance = DISPLAY_HEIGHT - (LOADING_ANIMATION_CENTER_Y - CORONA_OUTER_RADIUS - 1.0f);
    return distance * t * t;
}

/** @brief The moon's centre relative to the sun for an approach of 0-1: constant speed on a shallow arc. */
static void moon_offset (float t, float *x, float *y) {
    float distance = SUN_RADIUS + MOON_RADIUS + MOON_START_GAP;
    float length = sqrtf(MOON_APPROACH_X * MOON_APPROACH_X + MOON_APPROACH_Y * MOON_APPROACH_Y);
    float start_x = MOON_APPROACH_X * distance / length;
    float start_y = MOON_APPROACH_Y * distance / length;
    if (t <= 0.0f) {
        *x = start_x;
        *y = start_y;
        return;
    }
    if (t >= 1.0f) {
        *x = 0.0f;
        *y = 0.0f;
        return;
    }
    float ux = -start_x / distance, uy = -start_y / distance;       // towards the sun
    float nx = uy, ny = -ux;                                        // bows to this side
    float arc_radius = (distance * distance) / (8.0f * MOON_ARC_BOW) + (MOON_ARC_BOW / 2.0f);
    float arc_angle = 2.0f * asinf(distance / (2.0f * arc_radius));
    float angle = (t - 0.5f) * arc_angle;
    float along = arc_radius * sinf(angle);
    float bow = arc_radius * (cosf(angle) - cosf(arc_angle / 2.0f));
    *x = (start_x / 2.0f) + ux * along + nx * bow;
    *y = (start_y / 2.0f) + uy * along + ny * bow;
}

/**
 * @brief Reset the animation before loading starts. (Nothing needs loading: it is drawn natively.)
 */
void ui_components_loading_animation_prepare (void) {
    started = false;
    done = false;
}

/**
 * @brief Reset the animation after loading.
 */
void ui_components_loading_animation_free (void) {
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
    return clamp01((get_ticks_ms() - start_ms) / (float) FADE_IN_MS);
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
    return done && (get_ticks_ms() - done_ms) >= CORONA_MS + DONE_HOLD_MS + DROP_MS;
}

/**
 * @brief Draw the animation for the given loading progress (0-1), over whatever is on screen.
 */
void ui_components_loading_animation_draw (float progress) {
    uint32_t now = get_ticks_ms();
    if (!started) {
        started = true;
        start_ms = now;
    }
    progress = clamp01(progress);

    // The colour behind the eclipse: the palette background, which the loading screen fades to
    // black from LOADING_FADE_START (load_rom.c), and black once loading is done.
    float screen_fade = done ? 1.0f : clamp01((progress - LOADING_FADE_START) / (1.0f - LOADING_FADE_START));
    color_t behind = mix(PALETTE_BACKGROUND, RGBA32(0x00, 0x00, 0x00, 0xFF), screen_fade);

    // Where the moon is (0: start, 1: over the sun), how far everything has faded in, and the corona.
    float fade = ui_components_loading_animation_fade_in();
    float approach = 0.0f;
    float corona = 0.0f;
    float drop_ms = -1.0f;  // time into the drop (< 0: not dropping)
    if (done) {
        uint32_t since = now - done_ms;
        approach = 1.0f;
        corona = clamp01((since + FRAME_MS) / (float) CORONA_MS);
        if (since > CORONA_MS + DONE_HOLD_MS) {
            drop_ms = since - CORONA_MS - DONE_HOLD_MS;
        }
    } else if (now - start_ms >= FADE_IN_MS) {
        approach = progress;
    }

    float moon_x, moon_y;
    moon_offset(approach, &moon_x, &moon_y);
    float distance = sqrtf(moon_x * moon_x + moon_y * moon_y);
    float p = clamp01(1.0f - distance / (2.0f * SUN_RADIUS));
    p = p * p * (3.0f - 2.0f * p);      // how far the eclipse is: drives the colours

    float cx = LOADING_ANIMATION_CENTER_X;
    float drop = (drop_ms >= 0.0f) ? drop_offset(drop_ms) : 0.0f;
    float cy = LOADING_ANIMATION_CENTER_Y + drop;

    rdpq_mode_push();
        rdpq_set_mode_standard();
        rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
        rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);

        // The sun, until the moon covers it exactly (its edge would show around the moon's).
        if (distance > 0.0f) {
            draw_ring(cx, cy, -1.0f, SUN_RADIUS, mix(behind, colour_lerp(style.sun_start, style.sun_end, p), fade));
        }
        // The moon, going from grey to the colour behind it.
        color_t moon = mix(behind, colour_lerp(style.moon_start, behind, p), fade);
        if (distance > 0.0f || moon.r != behind.r || moon.g != behind.g || moon.b != behind.b) {
            draw_ring(cx + moon_x, cy + moon_y, -1.0f, MOON_RADIUS, moon);
        }
        if (corona > 0.0f) {
            color_t ring = mix(behind, style.corona, corona * fade);
            // Motion blur: fading copies where the ring was during the last frame, oldest first,
            // so each newer copy covers the older ones and the trail fades out behind it.
            if (drop_ms > 0.0f) {
                float trail = drop - drop_offset(drop_ms - BLUR_SHUTTER_MS);
                int copies = MIN(BLUR_MAX_COPIES, (int) ceilf(trail / BLUR_SPACING_PX));
                for (int i = copies; i >= 1; i--) {
                    float ghost_drop = drop_offset(drop_ms - (BLUR_SHUTTER_MS * i) / (float) copies);
                    float strength = 1.0f - i / (float) (copies + 1);
                    draw_ring(cx, LOADING_ANIMATION_CENTER_Y + ghost_drop, CORONA_INNER_RADIUS, CORONA_OUTER_RADIUS,
                        mix(behind, ring, strength));
                }
            }
            draw_ring(cx, cy, CORONA_INNER_RADIUS, CORONA_OUTER_RADIUS, ring);
        }
    rdpq_mode_pop();
}
