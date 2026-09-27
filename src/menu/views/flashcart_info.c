/**
 * @file flashcart_info.c
 * @brief Flashcart Information screen (Settings tab): cart type, firmware, features and live diagnostics
 * @ingroup view
 */

#include "views.h"
#include "../sound.h"
#include "../ui_components/constants.h"
#include <libcart/cart.h>


static const char *yes_no (bool value) {
    return value ? "Yes" : "No";
}

static const char *cart_value (menu_t *menu) {
    return (cart_type == CART_SC) ? "SummerCart64" : "None detected";
}

static const char *firmware_value (menu_t *menu) {
    static char buffer[24];
    flashcart_firmware_version_t version = flashcart_get_firmware_version();
    snprintf(buffer, sizeof(buffer), "%u.%u.%lu", version.major, version.minor, version.revision);
    return buffer;
}

static const char *feature_64dd (menu_t *menu) { return yes_no(flashcart_has_feature(FLASHCART_FEATURE_64DD)); }
static const char *feature_rtc (menu_t *menu) { return yes_no(flashcart_has_feature(FLASHCART_FEATURE_RTC)); }
static const char *feature_usb (menu_t *menu) { return yes_no(flashcart_has_feature(FLASHCART_FEATURE_USB)); }
static const char *feature_cic (menu_t *menu) { return yes_no(flashcart_has_feature(FLASHCART_FEATURE_AUTO_CIC)); }
static const char *feature_region (menu_t *menu) { return yes_no(flashcart_has_feature(FLASHCART_FEATURE_AUTO_REGION)); }
static const char *feature_writeback (menu_t *menu) { return yes_no(flashcart_has_feature(FLASHCART_FEATURE_SAVE_WRITEBACK)); }
static const char *feature_update (menu_t *menu) { return yes_no(flashcart_has_feature(FLASHCART_FEATURE_BIOS_UPDATE_FROM_MENU)); }
static const char *feature_reboot (menu_t *menu) { return yes_no(flashcart_has_feature(FLASHCART_FEATURE_ROM_REBOOT_FAST)); }

// Live values, read every frame.
static const char *button_value (menu_t *menu) {
    bool pressed = false;
    if (!flashcart_has_button_state() || flashcart_get_button_state(&pressed) != FLASHCART_OK) {
        return "Unavailable";
    }
    return pressed ? "Pressed" : "Released";
}

static const char *diagnostics_value (menu_t *menu) {
    static char buffer[32];
    uint16_t voltage_mv;
    int16_t temperature_deci_c;

    if (!flashcart_has_voltage_temperature() ||
        flashcart_get_voltage_temperature(&voltage_mv, &temperature_deci_c) != FLASHCART_OK) {
        return "Unavailable";
    }

    int temperature_abs = (temperature_deci_c < 0) ? -temperature_deci_c : temperature_deci_c;
    snprintf(buffer, sizeof(buffer), "%u mV / %s%d.%d C",
        voltage_mv, (temperature_deci_c < 0) ? "-" : "", temperature_abs / 10, temperature_abs % 10);
    return buffer;
}

static option_t options[] = {
    { .label = "Flashcart", .type = OPTION_INFO, .value = cart_value },
    { .label = "Firmware", .type = OPTION_INFO, .value = firmware_value,
      .description = "SummerCart64 firmware 2.17.0 or newer is required." },
    { .label = "64DD Emulation", .type = OPTION_INFO, .value = feature_64dd },
    { .label = "Real-Time Clock", .type = OPTION_INFO, .value = feature_rtc },
    { .label = "USB Debugging", .type = OPTION_INFO, .value = feature_usb },
    { .label = "Automatic CIC", .type = OPTION_INFO, .value = feature_cic },
    { .label = "Region Detection", .type = OPTION_INFO, .value = feature_region },
    { .label = "Save Writeback", .type = OPTION_INFO, .value = feature_writeback,
      .description = "Saves are written to the SD card while you play, without pressing Reset." },
    { .label = "Firmware Updates", .type = OPTION_INFO, .value = feature_update },
    { .label = "Fast Reboot", .type = OPTION_INFO, .value = feature_reboot },
    { .label = "Button", .type = OPTION_INFO, .value = button_value,
      .description = "Live state of the button on the cartridge." },
    { .label = "Voltage / Temp", .type = OPTION_INFO, .value = diagnostics_value,
      .description = "Live readings from the cartridge." },
};

static option_list_t list = {
    .options = options,
    .count = sizeof(options) / sizeof(options[0]),
};


static void process (menu_t *menu) {
    if (ui_components_option_list_process(menu, &list)) {
        return;
    }

    if (menu->actions.back) {
        sound_play_effect(SFX_EXIT);
        menu->next_mode = MENU_MODE_SETTINGS_HUB;
    }
}

static void draw (menu_t *menu, surface_t *d) {
    rdpq_attach_clear(d, NULL);

    ui_components_option_screen_draw(menu, "Flashcart Information", &list);

    rdpq_detach_show();
}


void view_flashcart_info_init (menu_t *menu) {
    ui_components_option_list_init(&list);
}

void view_flashcart_info_display (menu_t *menu, surface_t *display) {
    process(menu);
    draw(menu, display);
}
