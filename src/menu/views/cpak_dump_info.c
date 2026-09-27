#include <stdbool.h>
#include <stdio.h>
#include <libdragon.h>
#include "views.h"
#include "../sound.h"
#include "../fonts.h"
#include "../ui_components/constants.h"
#include "utils/cpakfs_utils.h"


static char cpak_path[255];
static char failure_message[255];

#define CONTROLLERPAK_BANK_SIZE 32768

static bool restore_controller_pak(int controller) {
    snprintf(failure_message, sizeof(failure_message), " ");

    if (!has_cpak(controller)) {
        snprintf(failure_message, sizeof(failure_message), "No Controller Pak detected on controller %d!", controller + 1);
        return false;
    }

    cpakfs_unmount(controller);

    uint8_t *data = malloc(CONTROLLERPAK_BANK_SIZE);
    if (!data) {
        snprintf(failure_message, sizeof(failure_message), "Memory allocation failed!");
        return false;
    }

    FILE *fp = fopen(cpak_path, "rb");
    if (!fp) {
        snprintf(failure_message, sizeof(failure_message), "Failed to open file for reading!");
        free(data);
        return false;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        snprintf(failure_message, sizeof(failure_message), "Seek failed!");
        fclose(fp);
        free(data);
        return false;
    }
    long filesize = ftell(fp);
    if (filesize < 0) {
        snprintf(failure_message, sizeof(failure_message), "ftell failed!");
        fclose(fp);
        free(data);
        return false;
    }
    rewind(fp);

    int total_banks = (int)((filesize + CONTROLLERPAK_BANK_SIZE - 1) / CONTROLLERPAK_BANK_SIZE);

    int banks_on_device = cpak_probe_banks(controller);
    if (banks_on_device < 1) {
        snprintf(failure_message, sizeof(failure_message), "Cannot probe Controller Pak banks (err=%d)!", banks_on_device);
        fclose(fp);
        free(data);
        return false;
    }
    if (total_banks > banks_on_device) {
        snprintf(failure_message, sizeof(failure_message), "Dump file too large (%d banks) for controller (%d banks)!",
                total_banks, banks_on_device);
        fclose(fp);
        free(data);
        return false;
    }

    debugf("Restoring Controller Pak: %ld bytes (%d banks)\n", filesize, total_banks);

    for (int bank = 0; bank < total_banks; bank++) {
        size_t bytesRead = fread(data, 1, CONTROLLERPAK_BANK_SIZE, fp);
        if (bytesRead == 0 && ferror(fp)) {
            snprintf(failure_message, sizeof(failure_message), "Read error from dump file!");
            fclose(fp);
            free(data);
            return false;
        }
        if (bytesRead == 0 && feof(fp)) break; // empty trailing chunk (shouldn't happen)

        int written = cpak_write((joypad_port_t)controller, (uint8_t)bank, 0, data, bytesRead);
        if (written < 0) {
            snprintf(failure_message, sizeof(failure_message), "Failed to write bank %d to Controller Pak! errno=%d", bank, written);
            fclose(fp);
            free(data);
            return false;
        }
        if ((size_t)written != bytesRead) {
            snprintf(failure_message, sizeof(failure_message), "Short write on bank %d: wrote %d / %zu bytes", bank, written, bytesRead);
            fclose(fp);
            free(data);
            return false;
        }
    }

    fclose(fp);
    free(data);

    snprintf(failure_message, sizeof(failure_message), "Backup restored to the pak in controller %d.", controller + 1);
    return true;
}

/* ---- Restore screen (option list) ---- */

static int controller_selected;
static bool confirm_restore;
static bool pending_restore;
static char result[256];
static bool restored;

static const char *backup_value (menu_t *menu) {
    return path_last_get(menu->cpak_restore_path);
}

static const char *kind_value (menu_t *menu) {
    return "Whole pak";
}

static void set_controller (menu_t *menu, void *arg) {
    controller_selected = (int) (uintptr_t) (arg);
}

static int get_controller_selection (menu_t *menu) {
    return controller_selected;
}

static component_context_menu_t controller_picker = {
    .get_default_selection = get_controller_selection,
    .list = {
        { .text = "Controller 1", .action = set_controller, .arg = (void *) (0) },
        { .text = "Controller 2", .action = set_controller, .arg = (void *) (1) },
        { .text = "Controller 3", .action = set_controller, .arg = (void *) (2) },
        { .text = "Controller 4", .action = set_controller, .arg = (void *) (3) },
        COMPONENT_CONTEXT_MENU_LIST_END,
    }
};

static void ask_restore (menu_t *menu) {
    confirm_restore = true;
}

static option_t options[] = {
    { .label = "Backup", .type = OPTION_INFO, .value = backup_value },
    { .label = "Type", .type = OPTION_INFO, .value = kind_value },
    { .label = "Controller", .type = OPTION_CHOICE, .picker = &controller_picker,
      .description = "The controller whose pak will be written." },
    { .label = "Restore", .type = OPTION_ACTION, .action = ask_restore,
      .description = "Write this backup to the Controller Pak." },
};

static option_list_t list = {
    .options = options,
    .count = sizeof(options) / sizeof(options[0]),
};

static void process (menu_t *menu) {
    if (result[0] != '\0') {
        if (menu->actions.enter || menu->actions.back) {
            result[0] = '\0';
            sound_play_effect(SFX_EXIT);
            if (restored) {
                menu->next_mode = MENU_MODE_CONTROLLER_PAKFS;
            }
        }
        return;
    }
    if (confirm_restore) {
        if (menu->actions.enter) {
            confirm_restore = false;
            pending_restore = true;
        } else if (menu->actions.back) {
            confirm_restore = false;
            sound_play_effect(SFX_EXIT);
        }
        return;
    }
    if (ui_components_option_list_process(menu, &list)) {
        return;
    }
    if (menu->actions.back) {
        sound_play_effect(SFX_EXIT);
        menu->next_mode = MENU_MODE_CONTROLLER_PAKFS;
    }
}

static void draw (menu_t *menu, surface_t *d) {
    ui_components_attach_clear(d);

    ui_components_option_screen_draw(menu, "Restore a Backup", &list);

    if (result[0] != '\0') {
        ui_components_messagebox_draw("%s", result);
    } else if (confirm_restore) {
        ui_components_messagebox_draw("Write this backup to the pak in controller %d?\n\nA: Restore    B: Cancel", controller_selected + 1);
    } else if (pending_restore) {
        ui_components_messagebox_draw("Working...");
    }

    rdpq_detach_show();

    // The restore runs after a frame that says it's in progress.
    if (pending_restore) {
        pending_restore = false;
        restored = restore_controller_pak(controller_selected);
        snprintf(result, sizeof(result), "%s", failure_message);
        sound_play_effect(restored ? SFX_SETTING : SFX_ERROR);
    }
}

void view_controller_pak_dump_info_init (menu_t *menu) {
    snprintf(cpak_path, sizeof(cpak_path), "%s", path_get(menu->cpak_restore_path));
    snprintf(failure_message, sizeof(failure_message), " ");
    controller_selected = 0;
    confirm_restore = false;
    pending_restore = false;
    restored = false;
    result[0] = '\0';
    ui_components_option_list_init(&list);
}

void view_controller_pak_dump_info_display (menu_t *menu, surface_t *display) {
    process(menu);
    draw(menu, display);
}
