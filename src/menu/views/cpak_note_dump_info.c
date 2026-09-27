#include <stdbool.h>
#include <stdio.h>
#include <libdragon.h>
#include "views.h"
#include "../sound.h"
#include "../fonts.h"
#include "../ui_components/constants.h"
#include "utils/cpakfs_utils.h"

static char cpak_note_path[255];
static char failure_message_note[255];

static bool restore_controller_pak_note(int controller) {
    snprintf(failure_message_note, sizeof(failure_message_note), " ");

    if (!has_cpak(controller)) {
        snprintf(failure_message_note, sizeof(failure_message_note), "No Controller Pak detected on controller %d!", controller + 1);
        return false;
    }

    char title[256];
    char filename_note[256];
    FILE *fSource, *fDestination;

    static cpakfs_stats_t cpakfs_stats; //to monitor free space

    extract_title_from_absolute_path(cpak_note_path, title, sizeof title);

    /* Always decode: the old code could not create backups for notes with FAT-invalid chars
     * so any usable legacy backup contains only FAT-safe chars and no %XX sequences.
     * A legacy note name that literally contains %XX (e.g. "save%2A") would be decoded
     * incorrectly; that is an accepted limitation without a separate metadata store. */
    char decoded_title[256];
    cpakfs_decode_fat_filename(decoded_title, title, sizeof(decoded_title));

    unmount_all_cpakfs();

    //mounting the CPAK:
    if (mount_cpakfs(controller) < 0){
        snprintf(failure_message_note, sizeof failure_message_note,
            "Failed to mount Controller Pak on controller %d!", controller + 1);
            return false;
    }
    
    cpakfs_get_stats(controller, &cpakfs_stats );

    //checking free space in the CPAK:
    int free_blocks = cpakfs_stats.pages.total - cpakfs_stats.pages.used;
    int free_notes = cpakfs_stats.notes.total - cpakfs_stats.notes.used;
    //debugf("Free blocks: %d blocks\n", free_blocks);
    //debugf("Free notes: %d notes\n", free_notes);

    if (free_notes <= 0) {
        snprintf(failure_message_note, sizeof(failure_message_note), "Not enough pages left on Controller Pak in controller %d!\n(Required: 1 / Available: 0)", controller + 1);
        cpakfs_unmount(controller);
        return false;
    }

    //debugf("Source filename: %s\n", cpak_note_path);

    //Opening the source file (the dump note file)
    fSource = fopen(cpak_note_path, "rb");
    if (fSource == NULL) {
        snprintf(failure_message_note, sizeof failure_message_note, "Failed to open source file: %s\n", cpak_note_path);
        cpakfs_unmount(controller);
        return false;
    }

    int size = get_block_size_from_fs_path(cpak_note_path);
    //debugf("Size in blocks: %d\n", size);

    if (size > free_blocks) {
        snprintf(failure_message_note, sizeof(failure_message_note), "Not enough space on Controller Pak in controller %d!\n(Required: %d / Available: %d)", controller + 1, size, free_blocks);
        fclose(fSource);
        cpakfs_unmount(controller);
        return false;
    }


    snprintf(filename_note, sizeof(filename_note), "%s%s", CPAK_MOUNT_ARRAY[controller], decoded_title);
    
    //debugf("Dest. filename: %s\n", filename_note);

    //Check if the file already exists, and if so, pick a unique name
    if (file_exists_full(filename_note)) {
        char unique_full[256];

        if (pick_unique_fullname_with_mount(CPAK_MOUNT_ARRAY[controller],
                                            decoded_title,
                                            unique_full, sizeof unique_full,
                                            file_exists_full) == 0)
        {
            snprintf(filename_note, sizeof(filename_note), "%s", unique_full);
            //debugf("File exists, new name picked: %s\n", filename_note);
        } else {
            cpakfs_unmount(controller);
            fclose(fSource);
            snprintf(failure_message_note, sizeof failure_message_note,
                     "Unable to pick a unique destination name for %s", title);
            return false;
        }
    }

    fDestination = fopen(filename_note, "wb");
    if (!fDestination) {
        fclose(fSource);
        cpakfs_unmount(controller);
        snprintf(failure_message_note, sizeof failure_message_note,
                 "Failed to open destination file: %s", filename_note);
        return false;
    }

    char buffer[4096];
    size_t bytesRead;

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), fSource)) > 0) {
        size_t bytesWritten = fwrite(buffer, 1, bytesRead, fDestination);
        if (bytesWritten < bytesRead) {
            //debugf("Write error while copying to destination!\n");
            fclose(fSource);
            fclose(fDestination);
            cpakfs_unmount(controller);
            return false;
        }
    }

    fclose(fSource);
    fclose(fDestination);

    cpakfs_unmount(controller);

    snprintf(failure_message_note, sizeof(failure_message_note), "Note restored to the pak in controller %d.", controller + 1);

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
    return "Note";
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

    ui_components_option_screen_draw(menu, "Restore a Note", &list);

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
        restored = restore_controller_pak_note(controller_selected);
        snprintf(result, sizeof(result), "%s", failure_message_note);
        sound_play_effect(restored ? SFX_SETTING : SFX_ERROR);
    }
}

void view_controller_pak_note_dump_info_init (menu_t *menu) {
    snprintf(cpak_note_path, sizeof(cpak_note_path), "%s", path_get(menu->cpak_restore_path));
    snprintf(failure_message_note, sizeof(failure_message_note), " ");
    controller_selected = 0;
    confirm_restore = false;
    pending_restore = false;
    restored = false;
    result[0] = '\0';
    ui_components_option_list_init(&list);
}

void view_controller_pak_note_dump_info_display (menu_t *menu, surface_t *display) {
    process(menu);
    draw(menu, display);
}
