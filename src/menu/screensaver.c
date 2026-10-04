/**
 * @file screensaver.c
 * @brief Screensaver: the spinning N64 logo bouncing around the screen after a while without input
 * @ingroup menu
 *
 * The logo moves diagonally on the palette's background and bounces off the edges of the screen
 * (inside the safe area), at the edges of the logo itself as it turns. Each bounce reverses
 * its spin; a corner hit counts as one bounce. Any button or stick movement wakes the menu without
 * acting on it, and input is ignored for a second after that, and until everything is let go.
 */

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

static int timeout_ms;
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
}


void screensaver_set_timeout (int seconds) {
    timeout_ms = seconds * 1000;
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

    ui_components_attach_clear(display);
    ui_components_n64_logo_draw((int) logo.x, (int) logo.y, frame, 0xFF);
    rdpq_detach_show();
}
