/**
 * @file screensaver.c
 * @brief Screensaver: the spinning N64 logo bouncing around the screen after a while without input
 * @ingroup menu
 *
 * The logo moves diagonally on the palette's background and bounces off the edges of the screen
 * (inside the safe area), at the edges of the logo itself as it turns. Each bounce reverses
 * its spin; a corner hit counts as one bounce. Any button or stick movement wakes the menu without
 * acting on it, and input is ignored for a second after that, and until everything is let go.
 *
 * Three styles (Menu Settings > Screensaver Style: one of them, or Random for a different one
 * each time; development builds default to SCREENSAVER_STYLE in the Makefile):
 *  - Bounce: the screen is cleared every frame, so one logo moves around.
 *  - Trails: the screen isn't cleared, so the logo leaves a trail of grey copies behind it, like
 *    the end of a game of Solitaire, and the screen is cleared every SCREENSAVER_TRAIL_CLEAR_MS.
 *    The menu has two screen buffers, drawn alternately, and each one got the copy it missed last
 *    frame in colour the frame before; so each frame redraws the last two copies in grey, then the
 *    new one in colour on top.
 *  - Grid: the screen filled with logos spinning together in place, every other column shifted
 *    down half a cell, the whole pattern drifting slowly diagonally (so nothing stays put on a CRT).
 */

#include <math.h>
#include <string.h>

#include <libdragon.h>

#include "n64_logo_frames.h"
#include "screensaver.h"
#include "ui_components.h"
#include "ui_components/constants.h"
#include "utils/utils.h"

#define SCREENSAVER_SPEED           (60.0f)     // pixels per second on each axis (2 a frame at 30fps)
#define SCREENSAVER_WAKE_MS         (1000)      // input ignored after waking up
#define SCREENSAVER_MAX_STEP_MS     (100)       // longest step the logo moves in one frame

#define SCREENSAVER_LEFT            (VISIBLE_AREA_X0)
#define SCREENSAVER_RIGHT           (VISIBLE_AREA_X1)
#define SCREENSAVER_TOP             (VISIBLE_AREA_Y0)
#define SCREENSAVER_BOTTOM          (VISIBLE_AREA_Y1)

#define SCREENSAVER_TRAIL_CLEAR_MS  (5 * 60 * 1000)

#define SCREENSAVER_GRID_GAP        (16)        // grid: space between logos
#define SCREENSAVER_GRID_DRIFT      (8.0f)      // grid: pixels per second on each axis
#define GRID_CELL_WIDTH             (N64_LOGO_WIDTH + SCREENSAVER_GRID_GAP)
#define GRID_CELL_HEIGHT            (N64_LOGO_HEIGHT + SCREENSAVER_GRID_GAP)

typedef enum {
    STYLE_BOUNCE,
    STYLE_TRAILS,
    STYLE_GRID,
    STYLE_COUNT,
} screensaver_style_t;

static int timeout_ms;
static screensaver_style_setting_t style_setting = SCREENSAVER_RANDOM;
static bool active;
static bool waking;             // just woken up: ignoring input
static uint32_t last_input_ms;  // last time there was input (or the screensaver couldn't start)
static uint32_t wake_ms;        // when it woke up

static struct {
    float x, y;                 // top-left corner of the logo's frame
    float dx, dy;               // pixels per second
    float phase;                // frame, 0 .. N64_LOGO_FRAMES
    int spin;                   // 1 or -1: which way it turns
    uint32_t last_ms;
} logo;

static struct {
    screensaver_style_t style;
    uint32_t frames;            // frames drawn (and copies stamped)
    struct { int x, y, image; } stamps[3];  // the last three copies, by frames % 3
    int clears;                 // screen buffers still to clear (both, at the start and every SCREENSAVER_TRAIL_CLEAR_MS)
    uint32_t cleared_ms;
    uint32_t started_ms;        // grid: drift and spin are timed from here
} trails;

/** @brief Grid style: logos spinning together in place, every other column half a cell lower, drifting. */
static void draw_grid (surface_t *display, uint32_t now) {
    uint32_t elapsed = now - trails.started_ms;
    int frame = (int) ((elapsed % N64_LOGO_LOOP_MS) * N64_LOGO_FRAMES / N64_LOGO_LOOP_MS);

    // The pattern repeats every two columns across and every cell down.
    float drift = (elapsed / 1000.0f) * SCREENSAVER_GRID_DRIFT;
    int offset_x = (int) fmodf(drift, 2.0f * GRID_CELL_WIDTH);
    int offset_y = (int) fmodf(drift, (float) GRID_CELL_HEIGHT);

    // The logos on screen (at most 5 columns of 4), drawn together so the texture loads once.
    int xs[32], ys[32];
    int count = 0;
    for (int column = -2; (column * GRID_CELL_WIDTH) + offset_x < DISPLAY_WIDTH; column++) {
        int x = (column * GRID_CELL_WIDTH) + offset_x;
        if (x + N64_LOGO_WIDTH <= 0) {
            continue;
        }
        int shift = (column & 1) ? (GRID_CELL_HEIGHT / 2) : 0;
        for (int row = -2; (row * GRID_CELL_HEIGHT) + offset_y + shift < DISPLAY_HEIGHT; row++) {
            int y = (row * GRID_CELL_HEIGHT) + offset_y + shift;
            if (y + N64_LOGO_HEIGHT > 0 && count < 32) {
                xs[count] = x;
                ys[count] = y;
                count++;
            }
        }
    }
    ui_components_attach_clear(display);
    ui_components_n64_logo_draw_many(xs, ys, count, frame);
    rdpq_detach_show();
}


/** @brief Whether any button is held or the stick is pushed, on any controller. */
static bool input_held (void) {
    JOYPAD_PORT_FOREACH (port) {
        if (joypad_get_buttons_held(port).raw || (joypad_get_direction(port, JOYPAD_2D_STICK) != JOYPAD_8WAY_NONE)) {
            return true;
        }
    }
    return false;
}

/** @brief Not over the boot animation or a game loading, or while changing screens. */
static bool can_start (menu_t *menu) {
    switch (menu->mode) {
        case MENU_MODE_NONE:
        case MENU_MODE_STARTUP:
        case MENU_MODE_LOAD_ROM:
        case MENU_MODE_BOOT:
            return false;
        default:
            return menu->next_mode == menu->mode;
    }
}

static void start (uint32_t now) {
    active = true;
    logo.x = (DISPLAY_WIDTH - N64_LOGO_WIDTH) / 2;
    logo.y = (DISPLAY_HEIGHT - N64_LOGO_HEIGHT) / 2;
    uint32_t r = TICKS_READ();
    logo.dx = (r & 1) ? SCREENSAVER_SPEED : -SCREENSAVER_SPEED;
    logo.dy = (r & 2) ? SCREENSAVER_SPEED : -SCREENSAVER_SPEED;
    logo.phase = ui_components_n64_logo_frame();    // carry on from the Library's logo
    logo.spin = 1;
    logo.last_ms = now;

    switch (style_setting) {
        case SCREENSAVER_BOUNCE: trails.style = STYLE_BOUNCE; break;
        case SCREENSAVER_TRAILS: trails.style = STYLE_TRAILS; break;
        case SCREENSAVER_GRID: trails.style = STYLE_GRID; break;
        default: trails.style = (screensaver_style_t) ((r >> 2) % STYLE_COUNT); break;
    }
    trails.started_ms = now;
    trails.frames = 0;
    trails.clears = 2;
    trails.cleared_ms = now;
}


void screensaver_set_timeout (int seconds) {
    timeout_ms = seconds * 1000;
}

static const char *style_keys[SCREENSAVER_STYLE_COUNT] = { "random", "bounce", "trails", "grid" };

const char *screensaver_style_key (screensaver_style_setting_t style) {
    return style_keys[(style < SCREENSAVER_STYLE_COUNT) ? style : SCREENSAVER_RANDOM];
}

screensaver_style_setting_t screensaver_style_from_key (const char *key) {
    for (int i = 0; key && i < SCREENSAVER_STYLE_COUNT; i++) {
        if (strcmp(key, style_keys[i]) == 0) {
            return (screensaver_style_setting_t) i;
        }
    }
    return SCREENSAVER_RANDOM;
}

void screensaver_set_style (screensaver_style_setting_t style) {
    style_setting = (style < SCREENSAVER_STYLE_COUNT) ? style : SCREENSAVER_RANDOM;
}

bool screensaver_update (menu_t *menu) {
    uint32_t now = get_ticks_ms();
    bool held = input_held();
    bool allowed = can_start(menu);

    if (active && (held || !allowed)) {
        active = false;
        waking = true;
        wake_ms = now;
    }
    if (held || !allowed) {
        last_input_ms = now;
    }
    if (waking && (now - wake_ms) >= SCREENSAVER_WAKE_MS && !held) {
        waking = false;
    }
    if (!active && !waking && allowed && (timeout_ms > 0) && ((now - last_input_ms) >= (uint32_t) timeout_ms)) {
        start(now);
    }

    if (active || waking) {
        memset(&menu->actions, 0, sizeof(menu->actions));
    }
    return active;
}

void screensaver_draw (surface_t *display) {
    uint32_t now = get_ticks_ms();
    float step = MIN(now - logo.last_ms, SCREENSAVER_MAX_STEP_MS) / 1000.0f;
    logo.last_ms = now;

    logo.x += logo.dx * step;
    logo.y += logo.dy * step;
    logo.phase += logo.spin * step * 1000.0f * N64_LOGO_FRAMES / N64_LOGO_LOOP_MS;
    while (logo.phase >= N64_LOGO_FRAMES) {
        logo.phase -= N64_LOGO_FRAMES;
    }
    while (logo.phase < 0) {
        logo.phase += N64_LOGO_FRAMES;
    }
    int frame = MIN((int) logo.phase, N64_LOGO_FRAMES - 1);

    // Bounce off the edges it's heading for, at the logo's visible edges in this frame.
    bool bounced = false;
    float left = logo.x + n64_logo_extents[frame].x0;
    float right = logo.x + n64_logo_extents[frame].x1;
    float top = logo.y + n64_logo_extents[frame].y0;
    float bottom = logo.y + n64_logo_extents[frame].y1;
    if (logo.dx < 0 && left <= SCREENSAVER_LEFT) {
        logo.x += SCREENSAVER_LEFT - left;
        logo.dx = -logo.dx;
        bounced = true;
    } else if (logo.dx > 0 && right >= SCREENSAVER_RIGHT) {
        logo.x -= right - SCREENSAVER_RIGHT;
        logo.dx = -logo.dx;
        bounced = true;
    }
    if (logo.dy < 0 && top <= SCREENSAVER_TOP) {
        logo.y += SCREENSAVER_TOP - top;
        logo.dy = -logo.dy;
        bounced = true;
    } else if (logo.dy > 0 && bottom >= SCREENSAVER_BOTTOM) {
        logo.y -= bottom - SCREENSAVER_BOTTOM;
        logo.dy = -logo.dy;
        bounced = true;
    }
    if (bounced) {
        logo.spin = -logo.spin;     // once, even in a corner
    }

    if (trails.style == STYLE_GRID) {
        draw_grid(display, now);
        return;
    }
    if (trails.style == STYLE_BOUNCE) {
        ui_components_attach_clear(display);
        ui_components_n64_logo_draw((int) logo.x, (int) logo.y, frame, 0xFF);
        rdpq_detach_show();
        return;
    }

    // Trails: draw over what's there.
    if ((now - trails.cleared_ms) >= SCREENSAVER_TRAIL_CLEAR_MS) {
        trails.clears = 2;
        trails.cleared_ms = now;
    }
    rdpq_attach(display, NULL);
    if (trails.clears > 0) {
        rdpq_clear(BACKGROUND_COLOR);
        trails.clears--;
    }
    // The last two copies in grey (this buffer has the older one in colour, and missed the newer),
    // then the new one in colour on top.
    for (uint32_t back = 2; back >= 1; back--) {
        if (trails.frames >= back) {
            int i = (trails.frames - back) % 3;
            ui_components_n64_logo_draw_gray(trails.stamps[i].x, trails.stamps[i].y, trails.stamps[i].image);
        }
    }
    int i = trails.frames % 3;
    trails.stamps[i].x = (int) logo.x;
    trails.stamps[i].y = (int) logo.y;
    trails.stamps[i].image = frame;
    ui_components_n64_logo_draw(trails.stamps[i].x, trails.stamps[i].y, frame, 0xFF);
    trails.frames++;
    rdpq_detach_show();
}
