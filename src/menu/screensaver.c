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
 *  - Starfield: logos fly out from the middle of the screen towards you, growing as they come and
 *    spinning together, fading in from the distance and quickly out as they pass; drawn far to
 *    near so near ones overlap.
 *  - Orbit: the logo, spinning slowly, with cartridges from the Library's folder circling it on a tilted
 *    ring (smaller and behind it at the back, larger and in front of it at the front). The ring's
 *    tilt and roll change slowly and its centre traces a small figure 8. It starts with games whose
 *    labels the carousel has cached; each time a cartridge passes the back, it's swapped for another
 *    game (its label read then, out of sight behind the logo).
 */

#include <math.h>
#include <string.h>

#include <libdragon.h>

#include "labels.h"
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

#define STARFIELD_STARS             (16)
#define STARFIELD_FAR               (8.0f)      // starfield: depth logos start at...
#define STARFIELD_NEAR              (0.5f)      // ...and pass the screen at
#define STARFIELD_SPEED             (1.4f)      // depth per second
#define STARFIELD_FOCAL             (320.0f)    // pixels per unit across at depth 1 (where a logo is full size)
#define STARFIELD_SPREAD_X          (2.8f)      // logos start this far out from the middle, at most
#define STARFIELD_SPREAD_Y          (1.2f)
#define STARFIELD_FADE_DEPTH        (2.5f)      // they fade in over this much depth after starting...
#define STARFIELD_FADE_OUT_DEPTH    (STARFIELD_FADE_DEPTH / 2.0f)   // ...and out over half that before passing the screen

#define ORBIT_CARTRIDGES            (12)
#define ORBIT_TURN_MS               (32000)     // one turn of the ring: a swap at the back every 3s
#define ORBIT_RADIUS                (288.0f)    // across, at the sides of the ring
#define ORBIT_LOGO_SPEED            (0.5f)      // the logo spins at this fraction of its usual speed
#define ORBIT_TILT_MIN              (0.18f)     // how far the ring is tipped towards you (sine of the angle), between these...
#define ORBIT_TILT_MAX              (0.42f)
#define ORBIT_TILT_MS               (47000)     // ...over this long
#define ORBIT_ROLL                  (0.18f)     // the ring leans up to this much either way (radians)...
#define ORBIT_ROLL_MS               (61000)     // ...over this long
#define ORBIT_CAMERA                (3.0f)      // camera distance from the ring's centre, in ring radii (perspective)
#define ORBIT_SCALE_SIDE            (0.68f)     // cartridge size at the sides of the ring (~0.5 at the back, ~1 at the front)
#define ORBIT_SHADE_BACK            (0.25f)     // cartridge brightness at the back of the ring (1 at the front): darker with distance;
                                                // labels go all the way to black there, so a swapped-in label comes in from black
#define ORBIT_EIGHT_X               (28.0f)     // the centre's figure 8: across...
#define ORBIT_EIGHT_Y               (14.0f)     // ...and down
#define ORBIT_EIGHT_MS              (40000)

typedef enum {
    STYLE_BOUNCE,
    STYLE_TRAILS,
    STYLE_GRID,
    STYLE_STARFIELD,
    STYLE_ORBIT,
    STYLE_COUNT,
} screensaver_style_t;

static menu_t *saver_menu;                  // for the Library's games (Orbit)

static struct {
    int count;
    struct {
        int entry;              // in the Library's list, -1 while waiting for its first trip round the back
        surface_t *label;
        bool owned;             // loaded here (freed here), rather than the carousel's cached one
        int turns;              // times it has passed the back
    } carts[ORBIT_CARTRIDGES];
} orbit;

static struct {
    float x, y, z;
} stars[STARFIELD_STARS];
static uint32_t star_seed;

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

/** @brief A random number from 0 to 1. */
static float random_unit (void) {
    star_seed = (star_seed * 1103515245u) + 12345u;
    return (float) ((star_seed >> 8) & 0xFFFF) / 65535.0f;
}

/** @brief A logo back to the far distance, somewhere around the middle. */
static void star_respawn (int i, float z) {
    stars[i].x = ((random_unit() * 2.0f) - 1.0f) * STARFIELD_SPREAD_X;
    stars[i].y = ((random_unit() * 2.0f) - 1.0f) * STARFIELD_SPREAD_Y;
    stars[i].z = z;
}

/** @brief Starfield style: logos flying out from the middle towards you. */
static void draw_starfield (surface_t *display, uint32_t now, float step) {
    uint32_t elapsed = now - trails.started_ms;
    int frame = (int) ((elapsed % N64_LOGO_LOOP_MS) * N64_LOGO_FRAMES / N64_LOGO_LOOP_MS);

    for (int i = 0; i < STARFIELD_STARS; i++) {
        stars[i].z -= STARFIELD_SPEED * step;
        // Past the screen (too close, or off an edge): back to the far distance.
        float scale = 1.0f / stars[i].z;
        float sx = DISPLAY_CENTER_X + (stars[i].x * STARFIELD_FOCAL * scale);
        float sy = DISPLAY_CENTER_Y + (stars[i].y * STARFIELD_FOCAL * scale);
        float half_w = (N64_LOGO_WIDTH * scale) / 2.0f;
        float half_h = (N64_LOGO_HEIGHT * scale) / 2.0f;
        if (stars[i].z < STARFIELD_NEAR || sx + half_w < 0 || sx - half_w > DISPLAY_WIDTH ||
                sy + half_h < 0 || sy - half_h > DISPLAY_HEIGHT) {
            star_respawn(i, STARFIELD_FAR);
        }
    }

    // Far to near, so nearer logos are drawn over farther ones.
    int order[STARFIELD_STARS];
    for (int i = 0; i < STARFIELD_STARS; i++) {
        int j = i;
        while (j > 0 && stars[order[j - 1]].z < stars[i].z) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = i;
    }

    ui_components_attach_clear(display);
    for (int n = 0; n < STARFIELD_STARS; n++) {
        int i = order[n];
        float scale = 1.0f / stars[i].z;
        float fade_in = (STARFIELD_FAR - stars[i].z) / STARFIELD_FADE_DEPTH;
        float fade_out = (stars[i].z - STARFIELD_NEAR) / STARFIELD_FADE_OUT_DEPTH;
        float fade = MIN(MIN(fade_in, fade_out), 1.0f);
        uint8_t alpha = (uint8_t) (MAX(0.0f, fade) * 0xFF);
        ui_components_n64_logo_draw_scaled(
            DISPLAY_CENTER_X + (stars[i].x * STARFIELD_FOCAL * scale),
            DISPLAY_CENTER_Y + (stars[i].y * STARFIELD_FOCAL * scale),
            frame, scale, alpha);
    }
    rdpq_detach_show();
}

static bool orbit_in_use (int entry) {
    for (int i = 0; i < orbit.count; i++) {
        if (orbit.carts[i].entry == entry) {
            return true;
        }
    }
    return false;
}

static void orbit_release (void) {
    for (int i = 0; i < orbit.count; i++) {
        if (orbit.carts[i].owned) {
            labels_free(orbit.carts[i].label);
        }
    }
    orbit.count = 0;
}

/** @brief Up to ORBIT_CARTRIDGES games from the Library's folder, those with cached labels first (nearest the selection). */
static void orbit_start (void) {
    orbit_release();
    menu_t *menu = saver_menu;
    if (!menu || !menu->browser.valid || menu->browser.entries <= 0) {
        return;
    }
    int entries = menu->browser.entries;
    int games = 0;
    for (int i = 0; i < entries; i++) {
        games += (menu->browser.list[i].type == ENTRY_TYPE_ROM);
    }
    int wanted = MIN(ORBIT_CARTRIDGES, games);
    int origin = MAX(0, menu->browser.selected);
    for (int k = 0; k < entries && orbit.count < wanted; k++) {
        int entry = (origin + ((k % 2) ? ((k + 1) / 2) : -(k / 2)) + entries) % entries;
        if (menu->browser.list[entry].type != ENTRY_TYPE_ROM || orbit_in_use(entry)) {
            continue;
        }
        surface_t *label = ui_components_carousel_cached_label(menu->browser.directory, menu->browser.list[entry].name);
        if (label) {
            orbit.carts[orbit.count++] = (typeof(orbit.carts[0])) { .entry = entry, .label = label, .owned = false };
        }
    }
    // The rest join on their first trip round the back.
    while (orbit.count < wanted) {
        orbit.carts[orbit.count++] = (typeof(orbit.carts[0])) { .entry = -1, .label = NULL, .owned = false };
    }
}

/** @brief Swap a cartridge for a random game not already on the ring (reads its label from the SD card). */
static void orbit_swap (int i) {
    menu_t *menu = saver_menu;
    int entries = menu->browser.entries;
    int start = (int) (random_unit() * entries) % entries;
    for (int k = 0; k < entries; k++) {
        int entry = (start + k) % entries;
        if (menu->browser.list[entry].type != ENTRY_TYPE_ROM || orbit_in_use(entry)) {
            continue;
        }
        if (orbit.carts[i].owned) {
            labels_free(orbit.carts[i].label);
        }
        orbit.carts[i].entry = entry;
        orbit.carts[i].label = ui_components_cartridge_label_load(menu->browser.directory, menu->browser.list[entry].name);
        orbit.carts[i].owned = true;
        return;
    }
    // Every game is already on the ring: keep this one (unless it hasn't joined yet).
}

/** @brief Orbit style: cartridges circling the still logo on a tilted, leaning, wandering ring. */
static void draw_orbit (surface_t *display, uint32_t now) {
    const float tau = 6.28318531f;
    uint32_t elapsed = now - trails.started_ms;
    float turn = (float) elapsed / ORBIT_TURN_MS;
    float tilt = ORBIT_TILT_MIN + ((ORBIT_TILT_MAX - ORBIT_TILT_MIN) * 0.5f * (1.0f - cosf(tau * elapsed / ORBIT_TILT_MS)));
    float roll = ORBIT_ROLL * sinf(tau * elapsed / ORBIT_ROLL_MS);
    float eight = tau * elapsed / ORBIT_EIGHT_MS;
    float ox = DISPLAY_CENTER_X + (ORBIT_EIGHT_X * sinf(eight));
    float oy = DISPLAY_CENTER_Y + (ORBIT_EIGHT_Y * sinf(2.0f * eight));

    uint32_t logo_loop = (uint32_t) (N64_LOGO_LOOP_MS / ORBIT_LOGO_SPEED);
    int logo_frame = (int) ((elapsed % logo_loop) * N64_LOGO_FRAMES / logo_loop);

    float xs[ORBIT_CARTRIDGES], ys[ORBIT_CARTRIDGES], depths[ORBIT_CARTRIDGES], scales[ORBIT_CARTRIDGES];
    float nearness[ORBIT_CARTRIDGES];   // 0 at the back of the ring, 1 at the front
    for (int i = 0; i < orbit.count; i++) {
        // Turns so far, counted from the back of the ring (an angle of -90 degrees).
        float phase = turn + ((float) i / orbit.count);
        int turns = (int) floorf(phase + 0.25f);
        if (turns != orbit.carts[i].turns) {
            orbit.carts[i].turns = turns;
            if (elapsed > 0) {
                orbit_swap(i);
            }
        }
        // A point on a ring of radius 1, tipped towards the camera, seen in perspective: nearer
        // cartridges are further apart on screen as well as larger.
        float angle = tau * phase;
        float x = cosf(angle);
        float z = sinf(angle);                          // towards the camera
        float y = z * tilt;                             // the front of the ring is lower
        float toward = z * sqrtf(1.0f - (tilt * tilt));
        float perspective = ORBIT_CAMERA / (ORBIT_CAMERA - toward);
        float rx = ORBIT_RADIUS * x * perspective;
        float ry = ORBIT_RADIUS * y * perspective;
        xs[i] = ox + (rx * cosf(roll)) - (ry * sinf(roll));
        ys[i] = oy + (rx * sinf(roll)) + (ry * cosf(roll));
        depths[i] = toward;
        nearness[i] = (z + 1.0f) / 2.0f;
        scales[i] = ORBIT_SCALE_SIDE * perspective;
    }

    // Back to front, with the logo between the two halves of the ring.
    int order[ORBIT_CARTRIDGES];
    for (int i = 0; i < orbit.count; i++) {
        int j = i;
        while (j > 0 && depths[order[j - 1]] > depths[i]) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = i;
    }

    ui_components_attach_clear(display);
    bool logo_drawn = false;
    for (int n = 0; n < orbit.count; n++) {
        int i = order[n];
        if (!logo_drawn && depths[i] >= 0.0f) {
            ui_components_n64_logo_draw((int) (ox - (N64_LOGO_WIDTH / 2)), (int) (oy - (N64_LOGO_HEIGHT / 2)), logo_frame, 0xFF);
            logo_drawn = true;
        }
        if (orbit.carts[i].entry < 0) {
            continue;   // joins on its first trip round the back
        }
        float shade = ORBIT_SHADE_BACK + ((1.0f - ORBIT_SHADE_BACK) * nearness[i]);
        ui_components_cartridge_draw(xs[i], ys[i], scales[i], orbit.carts[i].label,
            (uint8_t) (MIN(1.0f, nearness[i]) * 0xFF), (uint8_t) (MIN(1.0f, shade) * 0xFF));
    }
    if (!logo_drawn) {
        ui_components_n64_logo_draw((int) (ox - (N64_LOGO_WIDTH / 2)), (int) (oy - (N64_LOGO_HEIGHT / 2)), logo_frame, 0xFF);
    }
    rdpq_detach_show();
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
        case SCREENSAVER_STARFIELD: trails.style = STYLE_STARFIELD; break;
        case SCREENSAVER_ORBIT: trails.style = STYLE_ORBIT; break;
        default: trails.style = (screensaver_style_t) ((r >> 2) % STYLE_COUNT); break;
    }
    trails.started_ms = now;

    orbit_release();
    if (trails.style == STYLE_ORBIT) {
        orbit_start();
        for (int i = 0; i < orbit.count; i++) {
            orbit.carts[i].turns = (int) floorf(((float) i / orbit.count) + 0.25f);
        }
    }

    // Starfield: logos spread out in depth, so they arrive one after another.
    star_seed = r ^ 0x9E3779B9u;
    for (int i = 0; i < STARFIELD_STARS; i++) {
        star_respawn(i, STARFIELD_NEAR + ((STARFIELD_FAR - STARFIELD_NEAR) * (i + random_unit()) / STARFIELD_STARS));
    }
    trails.frames = 0;
    trails.clears = 2;
    trails.cleared_ms = now;
}


void screensaver_set_timeout (int seconds) {
    timeout_ms = seconds * 1000;
}

static const char *style_keys[SCREENSAVER_STYLE_COUNT] = { "random", "bounce", "trails", "grid", "starfield", "orbit" };

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

    saver_menu = menu;
    if (active && (held || !allowed)) {
        active = false;
        waking = true;
        wake_ms = now;
        orbit_release();
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
    if (trails.style == STYLE_STARFIELD) {
        draw_starfield(display, now, step);
        return;
    }
    if (trails.style == STYLE_ORBIT) {
        draw_orbit(display, now);
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
