/**
 * @file cpakfs_manager.c
 * @brief Controller Pak screen (Settings tab): back up, restore, format, and manage notes
 * @ingroup view
 *
 * An option list: the controller to use, the pak's status, whole-pak actions, then one row per
 * note (A opens Back Up / Delete). "Restore a Backup" lists the backups on the SD card and hands
 * the chosen file to the restore screens (cpak_dump_info.c / cpak_note_dump_info.c).
 */

#include <stdbool.h>
#include <stdio.h>
#include <strings.h>
#include <libdragon.h>
#include <errno.h>
#include <dir.h>
#include "views.h"
#include "../sound.h"
#include "../fonts.h"
#include "../ui_components/constants.h"
#include "utils/fs.h"
#include "utils/cpakfs_utils.h"

#define MAX_STRING_LENGTH   (62)
#define MEMPAK_BANK_SIZE    (32768)
#define MAX_BACKUPS         (48)

#define CPAK_EXTENSION      ".pak"
#define CPAK_NOTE_EXTENSION ".paknote"

static char *CPAK_PATH = "sd:/cpak_saves";
static char *CPAK_NOTES_PATH = "sd:/cpak_saves/notes";

typedef enum {
    OP_NONE,
    OP_BACKUP_PAK,
    OP_BACKUP_NOTE,
    OP_DELETE_NOTE,
    OP_FORMAT,
} operation_t;

static bool use_rtc;
static int controller_selected;

// Pak state for the selected controller, refreshed every frame.
static bool mounted[4];
static bool has_pak;
static bool corrupted;
static cpakfs_stats_t stats;

// Notes on the selected pak.
static int note_count;
static bool notes_loaded;
static char note_names[MAX_NUM_NOTES][MAX_STRING_LENGTH];
static cpakfs_path_strings_t note_parts[MAX_NUM_NOTES];
static char note_labels[MAX_NUM_NOTES][24];
static char note_values[MAX_NUM_NOTES][24];
static int note_selected;

static operation_t confirm_op = OP_NONE;    // destructive operation waiting for A / B
static operation_t pending_op = OP_NONE;    // operation to run after drawing a "working" frame
static char message[256];                   // result shown in a message box

// "Restore a Backup" list.
static bool restoring = false;
static int backup_count;
static char backup_names[MAX_BACKUPS][MAX_STRING_LENGTH];
static bool backup_is_note[MAX_BACKUPS];
static option_t backup_options[MAX_BACKUPS];
static option_list_t backup_list = { .options = backup_options };


/* ---- Pak access ---- */

static void unmount_all (void) {
    unmount_all_cpakfs();
    for (int i = 0; i < 4; i++) {
        mounted[i] = false;
    }
}

static void load_notes (void) {
    dir_t entry;
    note_count = 0;
    if (dir_findfirst(CPAK_MOUNT_ARRAY[controller_selected], &entry) >= 0) {
        do {
            snprintf(note_names[note_count], MAX_STRING_LENGTH, "%s", entry.d_name);
            parse_cpakfs_fullname(entry.d_name, &note_parts[note_count]);

            char full[256];
            snprintf(full, sizeof(full), "%s%s", CPAK_MOUNT_ARRAY[controller_selected], entry.d_name);
            int blocks = get_block_size_from_fs_path(full);

            snprintf(note_labels[note_count], sizeof(note_labels[0]), "%s",
                note_parts[note_count].filename[0] ? note_parts[note_count].filename : entry.d_name);
            snprintf(note_values[note_count], sizeof(note_values[0]), "%.4s  %d blocks",
                note_parts[note_count].gamecode, (blocks < 0) ? 0 : blocks);
            note_count++;
        } while (note_count < MAX_NUM_NOTES && dir_findnext(CPAK_MOUNT_ARRAY[controller_selected], &entry) == 0);
    }
    notes_loaded = true;
}

/** @brief Check the selected controller's pak (inserted, removed, readable) and mount it. */
static void refresh_pak (void) {
    bool present = (joypad_get_accessory_type(controller_selected) == JOYPAD_ACCESSORY_TYPE_CONTROLLER_PAK);

    if (!present) {
        if (mounted[controller_selected]) {
            cpakfs_unmount(controller_selected);
            mounted[controller_selected] = false;
        }
        if (has_pak) {
            notes_loaded = false;
        }
        has_pak = false;
        corrupted = false;
        note_count = 0;
        memset(&stats, 0, sizeof(stats));
        return;
    }

    if (!has_pak) {
        notes_loaded = false;
    }
    has_pak = true;

    if (!mounted[controller_selected]) {
        corrupted = (mount_cpakfs(controller_selected) < 0);
        mounted[controller_selected] = !corrupted;
    }
    if (!corrupted) {
        cpakfs_get_stats(controller_selected, &stats);
        if (!notes_loaded) {
            load_notes();
        }
    } else {
        note_count = 0;
    }
}

/** @brief Forget the mounted state so the pak is re-read (after it was changed). */
static void pak_changed (void) {
    cpakfs_unmount(controller_selected);
    mounted[controller_selected] = false;
    has_pak = false;
    notes_loaded = false;
}

static void timestamp (char *buffer, size_t size) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    snprintf(buffer, size, "%04d-%02d-%02d_%02d%02d%02d",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

static void backup_pak (void) {
    if (stats.pages.used <= 0) {
        snprintf(message, sizeof(message), "This Controller Pak is empty.\nThere is nothing to back up.");
        return;
    }

    int banks = cpak_probe_banks(controller_selected);
    if (banks < 1) {
        banks = 1;
    }

    char when[32];
    timestamp(when, sizeof(when));
    char filename[200];
    snprintf(filename, sizeof(filename), "%s/CPAK_%s%s", CPAK_PATH, when, CPAK_EXTENSION);

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        snprintf(message, sizeof(message), "Couldn't create the backup file on the SD card.");
        return;
    }

    uint8_t *bank = malloc(MEMPAK_BANK_SIZE);
    if (!bank) {
        fclose(fp);
        snprintf(message, sizeof(message), "Not enough memory to back up the pak.");
        return;
    }

    bool ok = true;
    for (int b = 0; b < banks && ok; b++) {
        int read = cpak_read((joypad_port_t) (controller_selected), (uint8_t) (b), 0, bank, MEMPAK_BANK_SIZE);
        if (read != MEMPAK_BANK_SIZE) {
            snprintf(message, sizeof(message), "Couldn't read the Controller Pak (bank %d).", b);
            ok = false;
        } else if (fwrite(bank, 1, MEMPAK_BANK_SIZE, fp) != MEMPAK_BANK_SIZE) {
            snprintf(message, sizeof(message), "Couldn't write the backup to the SD card.");
            ok = false;
        }
    }

    free(bank);
    fclose(fp);
    if (ok) {
        snprintf(message, sizeof(message), "Backup saved to\ncpak_saves/CPAK_%s.pak", when);
    }
}

static void backup_note (int index) {
    char source[256];
    snprintf(source, sizeof(source), "%s%s", CPAK_MOUNT_ARRAY[controller_selected], note_names[index]);

    FILE *in = fopen(source, "rb");
    if (!in) {
        snprintf(message, sizeof(message), "Couldn't read that note.");
        return;
    }

    char when[32];
    timestamp(when, sizeof(when));
    char safe_name[MAX_STRING_LENGTH];
    cpakfs_sanitize_fat_filename(safe_name, note_names[index], sizeof(safe_name));
    char destination[256];
    snprintf(destination, sizeof(destination), "%s/%s_%s%s", CPAK_NOTES_PATH, safe_name, when, CPAK_NOTE_EXTENSION);

    FILE *out = fopen(destination, "wb");
    if (!out) {
        fclose(in);
        snprintf(message, sizeof(message), "Couldn't create the backup file on the SD card.");
        return;
    }

    char buffer[4096];
    size_t bytes;
    bool ok = true;
    while ((bytes = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, bytes, out) != bytes) {
            ok = false;
            break;
        }
    }
    fclose(in);
    fclose(out);

    if (ok) {
        snprintf(message, sizeof(message), "\"%s\" saved to\ncpak_saves/notes", note_labels[index]);
    } else {
        snprintf(message, sizeof(message), "Couldn't write the backup to the SD card.");
    }
}

static void delete_note (int index) {
    char path[256];
    snprintf(path, sizeof(path), "%s%s", CPAK_MOUNT_ARRAY[controller_selected], note_names[index]);

    if (remove(path) != 0 && file_exists(path)) {
        snprintf(message, sizeof(message), "Couldn't delete \"%s\".", note_labels[index]);
        return;
    }
    snprintf(message, sizeof(message), "\"%s\" deleted.", note_labels[index]);
    pak_changed();
}

static void format_pak (void) {
    int result = cpakfs_format(controller_selected, false);
    if (result < 0) {
        snprintf(message, sizeof(message), "Couldn't format the Controller Pak (error %d).", result);
    } else {
        snprintf(message, sizeof(message), "Controller Pak formatted.");
    }
    pak_changed();
}

static void run_operation (operation_t op) {
    switch (op) {
        case OP_BACKUP_PAK: backup_pak(); break;
        case OP_BACKUP_NOTE: backup_note(note_selected); break;
        case OP_DELETE_NOTE: delete_note(note_selected); break;
        case OP_FORMAT: format_pak(); break;
        default: break;
    }
    sound_play_effect(SFX_SETTING);
}


/* ---- Restore a Backup list ---- */

static bool has_extension (const char *name, const char *a, const char *b) {
    const char *dot = strrchr(name, '.');
    return dot && (strcasecmp(dot, a) == 0 || strcasecmp(dot, b) == 0);
}

static void scan_folder (const char *folder, bool notes) {
    dir_t entry;
    if (dir_findfirst(folder, &entry) < 0) {
        return;
    }
    do {
        if (backup_count >= MAX_BACKUPS) {
            break;
        }
        bool match = notes ? has_extension(entry.d_name, CPAK_NOTE_EXTENSION, ".mpkn") : has_extension(entry.d_name, CPAK_EXTENSION, ".mpk");
        if (entry.d_type != DT_DIR && match) {
            snprintf(backup_names[backup_count], MAX_STRING_LENGTH, "%s", entry.d_name);
            backup_is_note[backup_count] = notes;
            backup_count++;
        }
    } while (dir_findnext(folder, &entry) == 0);
}

static const char *whole_pak_value (menu_t *menu) { return "Whole pak"; }
static const char *single_note_value (menu_t *menu) { return "Note"; }

static void open_backup (menu_t *menu) {
    int index = backup_list.selected;
    path_free(menu->cpak_restore_path);
    menu->cpak_restore_path = path_create(backup_is_note[index] ? CPAK_NOTES_PATH : CPAK_PATH);
    path_push(menu->cpak_restore_path, backup_names[index]);
    menu->next_mode = backup_is_note[index] ? MENU_MODE_CONTROLLER_PAK_DUMP_NOTE_INFO : MENU_MODE_CONTROLLER_PAK_DUMP_INFO;
}

static void open_restore_list (void) {
    backup_count = 0;
    scan_folder(CPAK_PATH, false);
    scan_folder(CPAK_NOTES_PATH, true);
    for (int i = 0; i < backup_count; i++) {
        backup_options[i] = (option_t) {
            .label = backup_names[i],
            .type = OPTION_ACTION,
            .action = open_backup,
            .value = backup_is_note[i] ? single_note_value : whole_pak_value,
        };
    }
    backup_list.count = backup_count;
    ui_components_option_list_init(&backup_list);
    restoring = true;
}


/* ---- Main option list ---- */

static void set_controller (menu_t *menu, void *arg) {
    int controller = (int) (uintptr_t) (arg);
    if (controller != controller_selected) {
        unmount_all();
        controller_selected = controller;
        has_pak = false;
        notes_loaded = false;
    }
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

static void note_backup_action (menu_t *menu, void *arg) {
    pending_op = OP_BACKUP_NOTE;
}

static void note_delete_action (menu_t *menu, void *arg) {
    confirm_op = OP_DELETE_NOTE;
}

static component_context_menu_t note_menu = {
    .list = {
        { .text = "Back Up Note", .action = note_backup_action },
        { .text = "Delete Note", .action = note_delete_action },
        COMPONENT_CONTEXT_MENU_LIST_END,
    }
};

static const char *status_value (menu_t *menu) {
    if (!has_pak) return "No Controller Pak";
    if (corrupted) return "Needs formatting";
    return "Controller Pak";
}

static const char *free_space_value (menu_t *menu) {
    static char buffer[48];
    if (!has_pak || corrupted) {
        return "-";
    }
    snprintf(buffer, sizeof(buffer), "%d of %d blocks, %d of %d notes",
        stats.pages.total - stats.pages.used, stats.pages.total,
        stats.notes.total - stats.notes.used, stats.notes.total);
    return buffer;
}

static const char *needs_pak_value (menu_t *menu) {
    return !has_pak ? "No Controller Pak" : (corrupted ? "Needs formatting" : NULL);
}

static const char *needs_clock_value (menu_t *menu) {
    return use_rtc ? needs_pak_value(menu) : "Needs the clock";
}

static void backup_pak_action (menu_t *menu) { pending_op = OP_BACKUP_PAK; }
static void restore_action (menu_t *menu) { open_restore_list(); }
static void format_action (menu_t *menu) { confirm_op = OP_FORMAT; }

static void note_action (menu_t *menu);

#define ROW_CONTROLLER  (0)
#define FIRST_NOTE_ROW  (6)

static option_t options[FIRST_NOTE_ROW + MAX_NUM_NOTES];
static option_list_t list = { .options = options };

/** @brief Rebuild the rows for the current pak (which rows can be used depends on it). */
static void build_options (void) {
    bool usable = has_pak && !corrupted;
    bool can_backup = usable && use_rtc;

    options[0] = (option_t) { .label = "Controller", .type = OPTION_CHOICE, .picker = &controller_picker,
        .description = "Which controller's pak to use." };
    options[1] = (option_t) { .label = "Status", .type = OPTION_INFO, .value = status_value };
    options[2] = (option_t) { .label = "Free Space", .type = OPTION_INFO, .value = free_space_value };
    options[3] = (option_t) { .label = "Back Up Whole Pak", .type = OPTION_ACTION,
        .action = can_backup ? backup_pak_action : NULL, .value = can_backup ? NULL : needs_clock_value,
        .description = "Save everything on the pak to cpak_saves on the SD card." };
    options[4] = (option_t) { .label = "Restore a Backup", .type = OPTION_ACTION, .action = restore_action,
        .description = "Write a saved pak or note back to a Controller Pak." };
    options[5] = (option_t) { .label = "Format Pak", .type = OPTION_ACTION,
        .action = has_pak ? format_action : NULL, .value = has_pak ? NULL : needs_pak_value,
        .description = "Erase everything on the pak." };

    int count = FIRST_NOTE_ROW;
    if (usable) {
        for (int i = 0; i < note_count; i++) {
            // The value column (game code and size) is drawn by draw_note_values(): value
            // callbacks don't know which row they belong to.
            options[count++] = (option_t) { .label = note_labels[i], .type = OPTION_ACTION, .action = note_action,
                .description = "Back up or delete this note." };
        }
    }
    list.count = count;
}

static void note_action (menu_t *menu) {
    note_selected = list.selected - FIRST_NOTE_ROW;
    ui_components_context_menu_init(&note_menu);
    ui_components_context_menu_show(&note_menu);
}


/* ---- Input ---- */

static void process (menu_t *menu) {
    if (message[0] != '\0') {
        if (menu->actions.enter || menu->actions.back) {
            message[0] = '\0';
            sound_play_effect(SFX_EXIT);
        }
        return;
    }

    if (confirm_op != OP_NONE) {
        if (menu->actions.enter) {
            pending_op = confirm_op;
            confirm_op = OP_NONE;
        } else if (menu->actions.back) {
            confirm_op = OP_NONE;
            sound_play_effect(SFX_EXIT);
        }
        return;
    }

    if (ui_components_context_menu_process(menu, &note_menu)) {
        return;
    }

    if (restoring) {
        if (ui_components_option_list_process(menu, &backup_list)) {
            return;
        }
        if (menu->actions.back) {
            restoring = false;
            sound_play_effect(SFX_EXIT);
        }
        return;
    }

    refresh_pak();
    build_options();
    if (list.selected >= list.count) {
        list.selected = list.count - 1;
    }

    if (ui_components_option_list_process(menu, &list)) {
        return;
    }

    if (menu->actions.back) {
        unmount_all();
        sound_play_effect(SFX_EXIT);
        menu->next_mode = MENU_MODE_SETTINGS_HUB;
    }
}


/* ---- Drawing ---- */

static void draw_note_values (void) {
    // Note rows' values (game code and size), drawn in the value column next to each row.
    int visible = OPTION_LIST_VISIBLE_ROWS;
    for (int row = 0; row < visible && list.first_visible + row < list.count; row++) {
        int i = list.first_visible + row;
        if (i < FIRST_NOTE_ROW) {
            continue;
        }
        int y = SETTINGS_LIST_Y + (row * OPTION_LIST_ROW_PITCH);
        rdpq_text_printf(&(rdpq_textparms_t) { .style_id = (i == list.selected) ? STL_DEFAULT : STL_GRAY },
            FNT_DEFAULT, OPTION_LIST_VALUE_X, y, "%s", note_values[i - FIRST_NOTE_ROW]);
    }
}

static void draw (menu_t *menu, surface_t *d) {
    ui_components_attach_clear(d);

    if (restoring) {
        ui_components_option_screen_draw(menu, "Restore a Backup", &backup_list);
        if (backup_count == 0) {
            ui_components_text_draw(
                &(rdpq_textparms_t) { .style_id = STL_GRAY, .width = VISIBLE_AREA_X1 - CAROUSEL_SELECTED_X, .wrap = WRAP_WORD },
                FNT_DEFAULT, CAROUSEL_SELECTED_X, SETTINGS_LIST_Y,
                "No backups yet. Back up a pak or a note first; backups are kept in cpak_saves on the SD card."
            );
        }
    } else {
        ui_components_option_screen_draw(menu, "Controller Pak", &list);
        draw_note_values();
        ui_components_context_menu_draw(&note_menu);
    }

    if (message[0] != '\0') {
        ui_components_messagebox_draw("%s", message);
    } else if (confirm_op == OP_FORMAT) {
        ui_components_messagebox_draw("Format the Controller Pak?\nEverything on it will be erased.\n\nA: Format    B: Cancel");
    } else if (confirm_op == OP_DELETE_NOTE) {
        ui_components_messagebox_draw("Delete \"%s\"?\n\nA: Delete    B: Cancel", note_labels[note_selected]);
    } else if (pending_op != OP_NONE) {
        ui_components_messagebox_draw("Working...");
    }

    rdpq_detach_show();

    // Slow operations run after a frame that says they're in progress.
    if (pending_op != OP_NONE) {
        operation_t op = pending_op;
        pending_op = OP_NONE;
        run_operation(op);
    }
}


void view_controller_pakfs_init (menu_t *menu) {
    unmount_all();
    controller_selected = 0;
    has_pak = false;
    corrupted = false;
    notes_loaded = false;
    note_count = 0;
    confirm_op = OP_NONE;
    pending_op = OP_NONE;
    message[0] = '\0';
    restoring = false;

    use_rtc = (menu->current_time >= 0);

    directory_create(CPAK_PATH);
    directory_create(CPAK_NOTES_PATH);

    refresh_pak();
    build_options();
    ui_components_option_list_init(&list);
    ui_components_context_menu_init(&note_menu);
}

void view_controller_pakfs_display (menu_t *menu, surface_t *display) {
    process(menu);
    draw(menu, display);
}
