/**
 * @file loading_animation.c
 * @brief Game loading animation: the boot animation's eclipse, driven by loading progress
 * @ingroup ui_components
 *
 * Uses the boot animation's frames (without the title): the sun and moon fade in, the moon's
 * approach follows the loading progress, and the corona appears once loading is done, holds, then
 * fades out so the game starts from black rather than cutting away from the ring. The eclipse
 * is centred on the bottom-right quadrant of the screen, over the Library.
 *
 * On the SummerCart64 the game is loaded over the cartridge space the menu's own files (rom:/)
 * live in, so every frame is loaded into memory by ui_components_loading_animation_prepare()
 * before loading starts. Every other approach frame is skipped to halve that (~170 KiB).
 */

#include "../ui_components.h"
#include "../boot_animation_frames.h"
#include "constants.h"
#include "utils/utils.h"

// Frame numbers (1-based) in the boot animation.
#define FADE_IN_LAST        (9)     // 1-9: sun and moon fade in, moon at its start
#define APPROACH_FIRST      (10)    // 10-54: moon moves over the sun
#define APPROACH_LAST       (54)
#define CORONA_LAST         (57)    // 55-57: corona fades in
#define FRAME_MS            (1000 / BOOT_ANIMATION_FPS)
#define DONE_HOLD_MS        (150)   // show the full corona this long,
#define RING_FADE_MS        (200)   // then fade it out (after loading: 100% is still totality)

// The sun's centre in the boot animation (make_eclipse_intro.py: SUN_CENTRE = 213.5, 200).
#define SUN_X               (213)
#define SUN_Y               (200)

static sprite_t *sprites[BOOT_ANIMATION_IMAGES];
static bool started;
static uint32_t start_ms;
static bool done;
static uint32_t done_ms;


static bool frame_used (int frame) {
    if (frame > APPROACH_FIRST && frame < APPROACH_LAST) {
        return ((frame - APPROACH_FIRST) % 2) == 0;
    }
    return frame <= CORONA_LAST;
}

/**
 * @brief Load the frames into memory. Call before loading starts (rom:/ is overwritten after).
 */
void ui_components_loading_animation_prepare (void) {
    for (int frame = 1; frame <= CORONA_LAST; frame++) {
        int image = boot_animation_frames[frame - 1].image;
        if (frame_used(frame) && image >= 0 && !sprites[image]) {
            char path[32];
            snprintf(path, sizeof(path), "rom:/boot/%02d.sprite", image);
            sprites[image] = sprite_load(path);
        }
    }
    started = false;
    done = false;
}

/**
 * @brief Free the frames.
 */
void ui_components_loading_animation_free (void) {
    rspq_wait();    // the RDP may still be drawing one
    for (int i = 0; i < BOOT_ANIMATION_IMAGES; i++) {
        if (sprites[i]) {
            sprite_free(sprites[i]);
            sprites[i] = NULL;
        }
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
    uint32_t elapsed = get_ticks_ms() - start_ms;
    return MIN(1.0f, elapsed / (float) (FADE_IN_LAST * FRAME_MS));
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
    uint32_t corona_ms = (CORONA_LAST - APPROACH_LAST) * FRAME_MS;
    return done && (get_ticks_ms() - done_ms) >= corona_ms + DONE_HOLD_MS + RING_FADE_MS;
}

static int current_frame (float progress) {
    uint32_t now = get_ticks_ms();
    if (!started) {
        started = true;
        start_ms = now;
    }
    if (done) {
        return MIN(CORONA_LAST, APPROACH_LAST + 1 + (int) ((now - done_ms) / FRAME_MS));
    }
    uint32_t elapsed = now - start_ms;
    if (elapsed < FADE_IN_LAST * FRAME_MS) {
        return 1 + (int) (elapsed / FRAME_MS);
    }
    progress = (progress < 0.0f) ? 0.0f : ((progress > 1.0f) ? 1.0f : progress);
    int frame = APPROACH_FIRST + (int) (progress * (APPROACH_LAST - APPROACH_FIRST));
    while (!frame_used(frame)) {
        frame--;
    }
    return frame;
}

/**
 * @brief Draw the animation for the given loading progress (0-1), over whatever is on screen.
 */
void ui_components_loading_animation_draw (float progress) {
    int frame = current_frame(progress);
    int image = boot_animation_frames[frame - 1].image;
    if (image < 0 || !sprites[image]) {
        return;     // an all-black frame (the moon exactly over the sun)
    }
    int x = LOADING_ANIMATION_CENTER_X - SUN_X + boot_animation_frames[frame - 1].x;
    int y = LOADING_ANIMATION_CENTER_Y - SUN_Y + boot_animation_frames[frame - 1].y;
    rdpq_mode_push();
        rdpq_set_mode_copy(true);   // black is transparent
        rdpq_sprite_blit(sprites[image], x, y, NULL);

        // After the hold, fade the ring out (the screen around it is already black).
        uint32_t fade_start = ((CORONA_LAST - APPROACH_LAST) * FRAME_MS) + DONE_HOLD_MS;
        uint32_t since_done = done ? get_ticks_ms() - done_ms : 0;
        if (done && since_done > fade_start) {
            uint32_t fading = MIN(RING_FADE_MS, since_done - fade_start);
            rdpq_set_mode_standard();
            rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
            rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
            rdpq_set_prim_color(RGBA32(0x00, 0x00, 0x00, (uint8_t) ((fading * 0xFF) / RING_FADE_MS)));
            rdpq_fill_rectangle(x, y, x + sprites[image]->width, y + sprites[image]->height);
        }
    rdpq_mode_pop();
}
