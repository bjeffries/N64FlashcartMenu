/**
 * @file rtc.c
 * @brief Time screen (Settings tab): shows the real-time clock and lets it be adjusted
 * @ingroup view
 */

#include <stdbool.h>
#include <stdio.h>
#include <libdragon.h>
#include <sys/time.h>
#include "../sound.h"
#include "../ui_components/constants.h"
#include "views.h"

#define WRAP(x, min, max)  ({ \
    typeof(x) _x = x; typeof(min) _min = min; typeof(max) _max = max; \
    _x < _min ? _max : _x > _max ? _min : _x; \
})

#define YEAR_MIN 1996
#define YEAR_MAX 2095

typedef enum {
    RTC_EDIT_YEAR,
    RTC_EDIT_MONTH,
    RTC_EDIT_DAY,
    RTC_EDIT_HOUR,
    RTC_EDIT_MIN,
    RTC_EDIT_SEC,
    RTC_EDIT_COUNT,
} rtc_field_t;

static const char *FIELD_NAMES[RTC_EDIT_COUNT] = { "YEAR", "MONTH", "DAY", "HOUR", "MIN", "SEC" };

static struct tm rtc_tm = {0};
static bool is_editing_mode;
static rtc_field_t editing_field_type;


static void adjust_rtc_time (struct tm *t, int incr) {
    switch (editing_field_type) {
        case RTC_EDIT_YEAR: t->tm_year = WRAP(t->tm_year + incr, YEAR_MIN - 1900, YEAR_MAX - 1900); break;
        case RTC_EDIT_MONTH: t->tm_mon = WRAP(t->tm_mon + incr, 0, 11); break;
        case RTC_EDIT_DAY: t->tm_mday = WRAP(t->tm_mday + incr, 1, 31); break;
        case RTC_EDIT_HOUR: t->tm_hour = WRAP(t->tm_hour + incr, 0, 23); break;
        case RTC_EDIT_MIN: t->tm_min = WRAP(t->tm_min + incr, 0, 59); break;
        case RTC_EDIT_SEC: t->tm_sec = WRAP(t->tm_sec + incr, 0, 59); break;
        default: break;
    }
    // Recalculate day-of-week and day-of-year
    time_t timestamp = mktime(t);
    *t = *gmtime(&timestamp);
}

static void save_rtc_time (menu_t *menu) {
    if (rtc_get_source() == RTC_SOURCE_JOYBUS && rtc_is_source_available(RTC_SOURCE_JOYBUS)) {
        struct timeval new_time = { .tv_sec = mktime(&rtc_tm) };
        if (settimeofday(&new_time, NULL) != 0) {
            menu_show_error(menu, "Failed to set the clock");
        }
    } else {
        menu_show_error(menu, "This clock can't be set");
    }
}

static void process (menu_t *menu) {
    if (!is_editing_mode) {
        if (menu->actions.back) {
            sound_play_effect(SFX_EXIT);
            menu->next_mode = MENU_MODE_SETTINGS_HUB;
        } else if (menu->actions.enter && menu->current_time >= 0) {
            rtc_tm = *gmtime(&menu->current_time);
            editing_field_type = RTC_EDIT_YEAR;
            is_editing_mode = true;
            sound_play_effect(SFX_ENTER);
        }
        return;
    }

    if (menu->actions.go_left) {
        editing_field_type = (editing_field_type + RTC_EDIT_COUNT - 1) % RTC_EDIT_COUNT;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.go_right) {
        editing_field_type = (editing_field_type + 1) % RTC_EDIT_COUNT;
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.go_up) {
        adjust_rtc_time(&rtc_tm, +1);
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.go_down) {
        adjust_rtc_time(&rtc_tm, -1);
        sound_play_effect(SFX_CURSOR);
    } else if (menu->actions.enter) {
        save_rtc_time(menu);
        is_editing_mode = false;
        sound_play_effect(SFX_SETTING);
    } else if (menu->actions.back) {
        is_editing_mode = false;
        sound_play_effect(SFX_EXIT);
    }
}

/** @brief Draw the date and time large, one field at a time; the field being edited is white and underlined. */
static void draw_fields (struct tm *t, bool editing) {
    char values[RTC_EDIT_COUNT][8];
    snprintf(values[RTC_EDIT_YEAR], 8, "%04d", t->tm_year + 1900);
    snprintf(values[RTC_EDIT_MONTH], 8, "%02d", t->tm_mon + 1);
    snprintf(values[RTC_EDIT_DAY], 8, "%02d", t->tm_mday);
    snprintf(values[RTC_EDIT_HOUR], 8, "%02d", t->tm_hour);
    snprintf(values[RTC_EDIT_MIN], 8, "%02d", t->tm_min);
    snprintf(values[RTC_EDIT_SEC], 8, "%02d", t->tm_sec);
    static const char *separators[RTC_EDIT_COUNT] = { "-", "-", "", ":", ":", "" };

    int x = CAROUSEL_SELECTED_X;
    for (int i = 0; i < RTC_EDIT_COUNT; i++) {
        bool selected = editing && (i == editing_field_type);
        menu_font_style_t style = (!editing || selected) ? STL_DEFAULT : STL_GRAY;

        if (editing) {
            rdpq_text_printf(&(rdpq_textparms_t) { .style_id = selected ? STL_DEFAULT : STL_GRAY },
                FNT_SMALL, x, TIME_VALUE_Y - 36, "%s", FIELD_NAMES[i]);
        }
        rdpq_textmetrics_t metrics = rdpq_text_printf(&(rdpq_textparms_t) { .style_id = style },
            FNT_TITLE, x, TIME_VALUE_Y, "%s", values[i]);
        if (selected) {
            ui_components_box_draw(x, TIME_VALUE_Y + 8, x + (int) (metrics.advance_x), TIME_VALUE_Y + 11, RGBA32(0xFF, 0xFF, 0xFF, 0xFF));
        }
        x += (int) (metrics.advance_x);

        metrics = rdpq_text_printf(&(rdpq_textparms_t) { .style_id = STL_GRAY },
            FNT_TITLE, x, TIME_VALUE_Y, "%s", separators[i]);
        x += (int) (metrics.advance_x) + ((i == RTC_EDIT_DAY) ? 28 : 0);
    }

    char weekday[16];
    strftime(weekday, sizeof(weekday), "%A", t);
    rdpq_text_printf(&(rdpq_textparms_t) { .style_id = STL_GRAY }, FNT_DEFAULT, CAROUSEL_SELECTED_X, TIME_VALUE_Y + 40, "%s", weekday);
}

static void draw (menu_t *menu, surface_t *d) {
    rdpq_attach_clear(d, NULL);

    rdpq_text_printf(NULL, FNT_DEFAULT, CAROUSEL_SELECTED_X, LIBRARY_HEADER_Y, "Time");

    int x = GAME_INFO_VALUE_X;
    if (menu->current_time < 0) {
        ui_components_text_draw(
            &(rdpq_textparms_t) { .style_id = STL_GRAY, .width = VISIBLE_AREA_X1 - CAROUSEL_SELECTED_X, .wrap = WRAP_WORD },
            FNT_DEFAULT, CAROUSEL_SELECTED_X, TIME_VALUE_Y,
            "No real-time clock was found."
        );
    } else {
        struct tm now = *gmtime(&menu->current_time);
        draw_fields(is_editing_mode ? &rtc_tm : &now, is_editing_mode);

        ui_components_text_draw(
            &(rdpq_textparms_t) { .style_id = STL_GRAY, .width = VISIBLE_AREA_X1 - CAROUSEL_SELECTED_X, .wrap = WRAP_WORD },
            FNT_SMALL, CAROUSEL_SELECTED_X, TIME_HELP_Y,
            is_editing_mode
                ? "Left / Right: choose a field. Up / Down: change it."
                : "Games with a clock (like Animal Forest) and the Last Played dates use this time. "
                  "It can also be set from a PC over USB."
        );

        if (is_editing_mode) {
            x += ui_components_button_hint_draw(ICON_A, x, LIBRARY_BUTTONS_Y, "Save") + LIBRARY_HINT_GAP;
            ui_components_button_hint_draw(ICON_B, x, LIBRARY_BUTTONS_Y, "Cancel");
            rdpq_detach_show();
            return;
        }
        x += ui_components_button_hint_draw(ICON_A, x, LIBRARY_BUTTONS_Y, "Adjust") + LIBRARY_HINT_GAP;
    }
    ui_components_button_hint_draw(ICON_B, x, LIBRARY_BUTTONS_Y, "Back");

    rdpq_detach_show();
}


void view_rtc_init (menu_t *menu) {
    /* Resync the time from the hardware RTC */
    rtc_set_source(rtc_get_source());
    is_editing_mode = false;
    editing_field_type = RTC_EDIT_YEAR;
}

void view_rtc_display (menu_t *menu, surface_t *display) {
    process(menu);
    draw(menu, display);
}
