/**
 * @file startup.c
 * @brief Startup view: plays the Eclipse Cart boot animation at power-on, then opens the Library
 * @ingroup view
 *
 * The frames (boot_animation/, converted by scripts/make_boot_animation.py) are cropped CI8
 * sprites in rom:/boot/, loaded one at a time as they're needed so the animation doesn't hold
 * ~700 KiB of memory. Timing follows the clock, so a slow frame never slows the animation down.
 * It only plays on a cold boot (not when Reset brings you back from a game) and can't be skipped;
 * Menu Settings > Boot Animation turns it off.
 */

#include "utils/fs.h"
#include "views.h"
#include "../boot_animation_frames.h"

#define HOLD_MS     (700)   // show the finished title this long after the last frame
#define FADE_MS     (300)   // then fade to black over this long

static struct {
    bool playing;
    uint32_t start_ms;
    sprite_t *sprite;
    int image;
} animation;


static void animation_finish (menu_t *menu) {
    rspq_wait();    // the RDP may still be drawing the last sprite
    if (animation.sprite) {
        sprite_free(animation.sprite);
    }
    animation.sprite = NULL;
    animation.playing = false;
    menu->next_mode = MENU_MODE_BROWSER;
}

static void draw_animation (menu_t *menu, surface_t *d) {
    if (animation.start_ms == 0) {
        animation.start_ms = get_ticks_ms();    // start on the first drawn frame, after loading
    }
    uint32_t elapsed = get_ticks_ms() - animation.start_ms;
    uint32_t frames_ms = (BOOT_ANIMATION_FRAMES * 1000) / BOOT_ANIMATION_FPS;
    int frame = (int) ((elapsed * BOOT_ANIMATION_FPS) / 1000);
    if (frame >= BOOT_ANIMATION_FRAMES) {
        frame = BOOT_ANIMATION_FRAMES - 1;
    }

    int image = boot_animation_frames[frame].image;
    if (image != animation.image) {
        rspq_wait();
        if (animation.sprite) {
            sprite_free(animation.sprite);
            animation.sprite = NULL;
        }
        if (image >= 0) {
            char path[32];
            snprintf(path, sizeof(path), "rom:/boot/%02d.sprite", image);
            animation.sprite = sprite_load(path);
        }
        animation.image = image;
    }

    rdpq_attach_clear(d, NULL);
    if (animation.sprite) {
        rdpq_set_mode_copy(false);
        rdpq_sprite_blit(animation.sprite, boot_animation_frames[frame].x, boot_animation_frames[frame].y, NULL);

        if (elapsed > frames_ms + HOLD_MS) {
            uint32_t fade = elapsed - frames_ms - HOLD_MS;
            uint8_t alpha = (fade >= FADE_MS) ? 0xFF : (uint8_t) ((fade * 0xFF) / FADE_MS);
            rdpq_set_mode_standard();
            rdpq_mode_combiner(RDPQ_COMBINER_FLAT);
            rdpq_mode_blender(RDPQ_BLENDER_MULTIPLY);
            rdpq_set_prim_color(RGBA32(0x00, 0x00, 0x00, alpha));
            rdpq_fill_rectangle(0, 0, d->width, d->height);
        }
    }
    rdpq_detach_show();

    if (elapsed >= frames_ms + HOLD_MS + FADE_MS) {
        animation_finish(menu);
    }
}

static void draw (menu_t *menu, surface_t *d) {
    if (animation.playing) {
        draw_animation(menu, d);
        return;
    }
    rdpq_attach_clear(d, NULL);
    rdpq_detach_show();
}


void view_startup_init (menu_t *menu) {
#ifdef FEATURE_AUTOLOAD_ROM_ENABLED
    // FIXME: rather than use a controller button, would it be better to use the cart button?
    JOYPAD_PORT_FOREACH (port) {
        joypad_poll();
        joypad_buttons_t b_held = joypad_get_buttons_held(port);

        if (menu->settings.rom_autoload_enabled && b_held.start) {
            menu->settings.rom_autoload_enabled = false;
            menu->settings.rom_autoload_path = "";
            menu->settings.rom_autoload_filename = "";
            settings_save(&menu->settings);
        }
    }
    if (menu->settings.rom_autoload_enabled) {
        menu->browser.directory = path_init(menu->storage_prefix, menu->settings.rom_autoload_path);
        menu->load.rom_path = path_clone_push(menu->browser.directory, menu->settings.rom_autoload_filename);
        menu->load_pending.rom_file = true;
        menu->next_mode = MENU_MODE_LOAD_ROM;

        return;
    }
#endif
    
    // Always open on the Library. (Upstream showed Menu Information on first run; it's in the
    // Settings tab now, and B from there would have landed on Settings instead of the Library.)
    if (menu->settings.first_run) {
        menu->settings.first_run = false;
        settings_save(&menu->settings);
    }

    // Power-on only: returning from a game with Reset is a warm (NMI) reset.
    if (menu->settings.boot_animation_enabled && sys_reset_type() == RESET_COLD) {
        animation.playing = true;
        animation.start_ms = 0;
        animation.sprite = NULL;
        animation.image = -1;
        return;     // draw() moves on to the Library when the animation ends
    }
    menu->next_mode = MENU_MODE_BROWSER;
}

void view_startup_display (menu_t *menu, surface_t *display) {
    draw(menu, display);
}
