#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "../cart_load.h"
#include "../fonts.h"
#include "../ui_components/constants.h"
#include "utils/fs.h"
#include "views.h"
#include "../sound.h"

static const char *cheat_extensions[] = {"cht", "cheats", "datel", "gameshark", NULL};
static const char *disk_extensions[] = { "ndd", NULL };
static const char *image_extensions[] = { "png", NULL };
static const char *n64_rom_extensions[] = { "z64", "n64", "v64", "rom", NULL };
static const char *save_extensions[] = { "sav", "eep", "sra", "srm", "fla", NULL };
static const char *text_extensions[] = { "txt", "ini", "yml", "yaml", NULL };
static const char *rom_meta_extensions[] = { "meta", "metadata", NULL };

// Fixed cap keeps memory use predictable on 4MB systems when scanning huge folders.
#define DIRECTORY_MAX_ENTRIES_JUMPER_PAK 1024

static bool directory_entry_limit_exceeded = false;
static int info_page = 0;

static const char *hidden_root_paths[] = {
    "/menu.bin",
    "/menu",
    "/N64FlashcartMenu.n64",
    "/sc64menu.n64",
    // Windows garbage
    "/System Volume Information",
    // macOS garbage
    "/.fseventsd",
    "/.Spotlight-V100",
    "/.Trashes",
    "/.VolumeIcon.icns",
    "/.metadata_never_index",
    NULL,
};

struct substr { const char *str; size_t len; };
#define substr(str) ((struct substr){ str, sizeof(str) - 1 })

static const struct substr hidden_basenames[] = {
    substr("desktop.ini"), // Windows Explorer settings
    substr("Thumbs.db"),   // Windows Explorer thumbnails
    substr(".DS_Store"),   // macOS Finder settings
};
#define HIDDEN_BASENAMES_COUNT (sizeof(hidden_basenames) / sizeof(hidden_basenames[0]))

static const struct substr hidden_prefixes[] = {
    substr("._"), // macOS "AppleDouble" metadata files
};
#define HIDDEN_PREFIXES_COUNT (sizeof(hidden_prefixes) / sizeof(hidden_prefixes[0]))

// static bool file_is_fat_hidden (const char *full_path) {
//     struct stat st;
    
//     if (stat(full_path, &st) == 0) {
//         return FAT_ATTR_IS_HID(&st);
//     }
    
//     return false;
// }

static bool path_is_hidden (path_t *path) {
    char *stripped_path = strip_fs_prefix(path_get(path));

    // Check for hidden files based on full path
    for (size_t i = 0; hidden_root_paths[i] != NULL; i++) {
        if (strcmp(stripped_path, hidden_root_paths[i]) == 0) {
            return true;
        }
    }

    char *basename = file_basename(stripped_path);
    size_t basename_len = strlen(basename);

    // Check for hidden files based on filename
    for (size_t i = 0; i < HIDDEN_BASENAMES_COUNT; i++) {
        if (basename_len == hidden_basenames[i].len &&
            strncmp(basename, hidden_basenames[i].str, hidden_basenames[i].len) == 0) {
            return true;
        }
    }
    
    // Check for hidden files based on filename prefix
    for (size_t i = 0; i < HIDDEN_PREFIXES_COUNT; i++) {
        if (basename_len > hidden_prefixes[i].len &&
            strncmp(basename, hidden_prefixes[i].str, hidden_prefixes[i].len) == 0) {
            return true;
        }
    }

    // if (file_is_fat_hidden(path_get(path))) {
    //     return true;
    // }

    return false;
}

static int compare_entry (const void *pa, const void *pb) {
    entry_t *a = (entry_t *) (pa);
    entry_t *b = (entry_t *) (pb);

    if (a->type != b->type) {
        if (a->type == ENTRY_TYPE_DIR) {
            return -1;
        } else if (b->type == ENTRY_TYPE_DIR) {
            return 1;
        } else if (a->type == ENTRY_TYPE_DISK) {
            return -1;
        } else if (b->type == ENTRY_TYPE_DISK) {
            return 1;
        } else if (a->type == ENTRY_TYPE_IMAGE) {
            return -1;
        } else if (b->type == ENTRY_TYPE_IMAGE) {
            return 1;
        } else if (a->type == ENTRY_TYPE_ROM) {
            return -1;
        } else if (b->type == ENTRY_TYPE_ROM) {
            return 1;
        } else if (a->type == ENTRY_TYPE_ROM_CHEAT) {
            return -1;
        } else if (b->type == ENTRY_TYPE_ROM_CHEAT) {
            return 1;
        } else if (a->type == ENTRY_TYPE_SAVE) {
            return -1;
        } else if (b->type == ENTRY_TYPE_SAVE) {
            return 1;
        } else if (a->type == ENTRY_TYPE_TEXT) {
            return -1;
        } else if (b->type == ENTRY_TYPE_TEXT) {
            return 1;
        } else if (a->type == ENTRY_TYPE_ROM_META) {
            return -1;
        } else if (b->type == ENTRY_TYPE_ROM_META) {
            return 1;
        }
    }

    return strcasecmp((const char *) (a->name), (const char *) (b->name));
}

static void browser_list_free (menu_t *menu) {
    ui_components_carousel_invalidate();

    for (int i = menu->browser.entries - 1; i >= 0; i--) {
        free(menu->browser.list[i].name);
    }

    free(menu->browser.list);

    menu->browser.list = NULL;
    menu->browser.list_capacity = 0;
    menu->browser.entries = 0;
    menu->browser.entry = NULL;
    menu->browser.selected = -1;
}

static bool browser_list_reserve(menu_t *menu, int32_t required) {
    if (required <= menu->browser.list_capacity) {
        return false;
    }

    int32_t new_capacity = menu->browser.list_capacity > 0 ? menu->browser.list_capacity : 32;
    while (new_capacity < required) {
        int32_t growth = new_capacity / 2;
        if (growth <= 0 || new_capacity > INT32_MAX - growth) {
            return true;
        }
        new_capacity += growth;
    }

    entry_t *grown = realloc(menu->browser.list, new_capacity * sizeof(entry_t));
    if (!grown) {
        return true;
    }

    menu->browser.list = grown;
    menu->browser.list_capacity = new_capacity;
    return false;
}

static bool load_directory (menu_t *menu) {
    int result;
    dir_t info;

    browser_list_free(menu);
    directory_entry_limit_exceeded = false;

    path_t *path = path_clone(menu->browser.directory);

    result = dir_findfirst(path_get(path), &info);

    while (result == 0) {
        bool hide = false;

        if (!menu->settings.show_protected_entries) {
            path_push(path, info.d_name);
            hide = path_is_hidden(path);
            path_pop(path);
        }

        if (!menu->settings.show_saves_folder) {
            path_push(path, info.d_name);
            // Skip the "saves" directory if it is hidden (this is case sensitive)
            if (strcmp(info.d_name, SAVE_DIRECTORY_NAME) == 0) {
                hide = true;
            }
            path_pop(path);
        }

        if (!menu->settings.show_save_files) {
            path_push(path, info.d_name);
            // Skip save files if they are hidden (this is case sensitive)
            if (file_has_extensions(info.d_name, save_extensions)) {
                hide = true;
            }
            path_pop(path);
        }

        if (!menu->settings.show_cheat_files) {
            path_push(path, info.d_name);
            // Skip cheat files if they are hidden (this is case sensitive)
            if (file_has_extensions(info.d_name, cheat_extensions)) {
                hide = true;
            }
            path_pop(path);
        }

        if (!hide) {
            if (!is_memory_expanded() && menu->browser.entries >= DIRECTORY_MAX_ENTRIES_JUMPER_PAK) {
                path_free(path);
                browser_list_free(menu);
                directory_entry_limit_exceeded = true;
                return true;
            }

            if (browser_list_reserve(menu, menu->browser.entries + 1)) {
                path_free(path);
                browser_list_free(menu);
                return true;
            }

            entry_t *entry = &menu->browser.list[menu->browser.entries];

            entry->name = strdup(info.d_name);
            if (!entry->name) {
                path_free(path);
                browser_list_free(menu);
                return true;
            }

            if (info.d_type == DT_DIR) {
                entry->type = ENTRY_TYPE_DIR;
            } else if (file_has_extensions(entry->name, n64_rom_extensions)) {
                entry->type = ENTRY_TYPE_ROM;
            } else if (file_has_extensions(entry->name, disk_extensions)) {
                entry->type = ENTRY_TYPE_DISK;
            } else if (file_has_extensions(entry->name, cheat_extensions)) {
                entry->type = ENTRY_TYPE_ROM_CHEAT;
            } else if (file_has_extensions(entry->name, save_extensions)) {
                entry->type = ENTRY_TYPE_SAVE;
            } else if (file_has_extensions(entry->name, image_extensions)) {
                entry->type = ENTRY_TYPE_IMAGE;
            } else if (file_has_extensions(entry->name, text_extensions)) {
                entry->type = ENTRY_TYPE_TEXT;
            } else if (file_has_extensions(entry->name, rom_meta_extensions)) {
                entry->type = ENTRY_TYPE_ROM_META;
            } else {
                entry->type = ENTRY_TYPE_OTHER;
            }

            // The Library carousel only shows folders and things that can be played.
            if (entry->type != ENTRY_TYPE_DIR && entry->type != ENTRY_TYPE_ROM && entry->type != ENTRY_TYPE_DISK) {
                free(entry->name);
                result = dir_findnext(path_get(path), &info);
                continue;
            }

            entry->size = info.d_size;
            entry->index = menu->browser.entries;
            menu->browser.entries++;
        }

        result = dir_findnext(path_get(path), &info);
    }

    path_free(path);

    if (result < -1) {
        browser_list_free(menu);
        return true;
    }

    if (menu->browser.entries > 0) {
        menu->browser.selected = 0;
        menu->browser.entry = &menu->browser.list[menu->browser.selected];
    }

    qsort(menu->browser.list, menu->browser.entries, sizeof(entry_t), compare_entry);

    return false;
}

static bool reload_directory (menu_t *menu) {
    int selected = menu->browser.selected;

    if (load_directory(menu)) {
        return true;
    }

    menu->browser.selected = selected;
    if (menu->browser.selected >= menu->browser.entries) {
        menu->browser.selected = menu->browser.entries - 1;
    }
    menu->browser.entry = menu->browser.selected >= 0 ? &menu->browser.list[menu->browser.selected] : NULL;

    return false;
}

static bool push_directory (menu_t *menu, char *directory) {
    path_t *previous_directory = path_clone(menu->browser.directory);

    path_push(menu->browser.directory, directory);

    if (load_directory(menu)) {
        path_free(menu->browser.directory);
        menu->browser.directory = previous_directory;
        return true;
    }

    path_free(previous_directory);

    return false;
}

static bool pop_directory (menu_t *menu) {
    path_t *previous_directory = path_clone(menu->browser.directory);

    path_pop(menu->browser.directory);

    if (load_directory(menu)) {
        path_free(menu->browser.directory);
        menu->browser.directory = previous_directory;
        return true;
    }

    for (uint16_t i = 0; i < menu->browser.entries; i++) {
        if (strcmp(menu->browser.list[i].name, path_last_get(previous_directory)) == 0) {
            menu->browser.selected = i;
            menu->browser.entry = &menu->browser.list[menu->browser.selected];
            break;
        }
    }

    path_free(previous_directory);

    return false;
}

static bool select_file (menu_t *menu, path_t *file) {
    path_t *previous_directory = path_clone(menu->browser.directory);

    path_free(menu->browser.directory);
    menu->browser.directory = path_clone(file);
    path_pop(menu->browser.directory);

    if (load_directory(menu)) {
        path_free(menu->browser.directory);
        menu->browser.directory = previous_directory;
        return true;
    }

    for (uint16_t i = 0; i < menu->browser.entries; i++) {
        if (strcmp(menu->browser.list[i].name, path_last_get(file)) == 0) {
            menu->browser.selected = i;
            menu->browser.entry = &menu->browser.list[menu->browser.selected];
            break;
        }
    }

    path_free(previous_directory);

    return false;
}

static void show_properties (menu_t *menu, void *arg) {
    menu->next_mode = MENU_MODE_FILE_INFO;
}

static void delete_entry (menu_t *menu, void *arg) {
    path_t *path = path_clone_push(menu->browser.directory, menu->browser.entry->name);

    if (remove(path_get(path))) {
        menu->browser.valid = false;
        if (menu->browser.entry->type == ENTRY_TYPE_DIR) {
            menu_show_error(menu, "Couldn't delete directory\nDirectory might not be empty");
        } else {
            menu_show_error(menu, "Couldn't delete file");
        }
        path_free(path);
        return;
    }

    path_free(path);

    if (reload_directory(menu)) {
        menu->browser.valid = false;
        menu_show_error(menu, "Couldn't refresh directory contents after delete operation");
    }
}

static void set_default_directory (menu_t *menu, void *arg) {
    free(menu->settings.default_directory);
    menu->settings.default_directory = strdup(strip_fs_prefix(path_get(menu->browser.directory)));
    settings_save(&menu->settings);
}

static component_context_menu_t entry_context_menu = {
    .list = {
        { .text = "Show entry properties", .action = show_properties },
        { .text = "Delete selected entry", .action = delete_entry },
        { .text = "Set current directory as default", .action = set_default_directory },
        COMPONENT_CONTEXT_MENU_LIST_END,
    }
};

static void set_menu_next_mode (menu_t *menu, void *arg) {
    menu_mode_t next_mode = (menu_mode_t) (arg);
    menu->next_mode = next_mode;
}

static component_context_menu_t settings_context_menu = {
    .list = {
        { .text = "Controller Pak manager", .action = set_menu_next_mode, .arg = (void *) (MENU_MODE_CONTROLLER_PAKFS) },
        { .text = "Menu settings", .action = set_menu_next_mode, .arg = (void *) (MENU_MODE_SETTINGS_EDITOR) },
        { .text = "Time (RTC) settings", .action = set_menu_next_mode, .arg = (void *) (MENU_MODE_RTC) },
        { .text = "Menu information", .action = set_menu_next_mode, .arg = (void *) (MENU_MODE_CREDITS) },
        { .text = "Flashcart information", .action = set_menu_next_mode, .arg = (void *) (MENU_MODE_FLASHCART) },
        { .text = "N64 information", .action = set_menu_next_mode, .arg = (void *) (MENU_MODE_SYSTEM_INFO) },
        COMPONENT_CONTEXT_MENU_LIST_END,
    }
};

static void process (menu_t *menu) {
    if (ui_components_context_menu_process(menu, &entry_context_menu)) {
        return;
    }

    if (ui_components_context_menu_process(menu, &settings_context_menu)) {
        return;
    }

    int scroll_speed = 1;

    // C-buttons also report a direction (go_fast); in the Library they are action buttons instead.
    if (menu->browser.entries > 1 && !menu->actions.go_fast) {
        if (menu->actions.go_left) {
            menu->browser.selected -= scroll_speed;
            if (menu->settings.wrap_file_list_scrolling) {
                // Wrap around to end if we go past the beginning
                menu->browser.selected = (menu->browser.selected % menu->browser.entries + menu->browser.entries) % menu->browser.entries;
            } else {
                // Clamp to beginning
                if (menu->browser.selected < 0) {
                    menu->browser.selected = 0;
                }
            }
            sound_play_effect(SFX_CURSOR);
        } else if (menu->actions.go_right) {
            menu->browser.selected += scroll_speed;
            if (menu->settings.wrap_file_list_scrolling) {
                // Wrap around to beginning if we go past the end
                menu->browser.selected = menu->browser.selected % menu->browser.entries;
            } else {
                // Clamp to end
                if (menu->browser.selected >= menu->browser.entries) {
                    menu->browser.selected = menu->browser.entries - 1;
                }
            }
            sound_play_effect(SFX_CURSOR);
        }
        menu->browser.entry = &menu->browser.list[menu->browser.selected];
    }

    if (menu->actions.enter && menu->browser.entry) {
        sound_play_effect(SFX_ENTER);
        switch (menu->browser.entry->type) {
            case ENTRY_TYPE_DIR:
                if (push_directory(menu, menu->browser.entry->name)) {
                    menu->browser.valid = false;
                    menu_show_error(
                        menu,
                        directory_entry_limit_exceeded
                            ? "Directory is too large for Jumper Pak\nUse an Expansion Pak"
                            : "Couldn't open next directory"
                    );
                }
                break;
            case ENTRY_TYPE_DISK:
                menu->next_mode = MENU_MODE_LOAD_DISK;
                break;
            case ENTRY_TYPE_IMAGE:
                menu->next_mode = MENU_MODE_IMAGE_VIEWER;
                break;
            case ENTRY_TYPE_ROM:
                menu->load.play_now = true;
                menu->next_mode = MENU_MODE_LOAD_ROM;
                break;
            case ENTRY_TYPE_ROM_CHEAT:
                menu->next_mode = MENU_MODE_FILE_INFO; // FIXME: Implement MENU_MODE_LOAD_ROM_CHEAT.
                break;
            case ENTRY_TYPE_TEXT:
                menu->next_mode = MENU_MODE_TEXT_VIEWER;
                break;
            case ENTRY_TYPE_ROM_META:
                menu->next_mode = MENU_MODE_FILE_INFO; // FIXME: Implement MENU_MODE_LOAD_ROM_META.
                break;

            default:
                menu->next_mode = MENU_MODE_FILE_INFO;
                break;
        }
    } else if (menu->actions.back && !path_is_root(menu->browser.directory)) {
        if (pop_directory(menu)) {
            menu->browser.valid = false;
            menu_show_error(
                menu,
                directory_entry_limit_exceeded
                    ? "Directory is too large for Jumper Pak\nUse an Expansion Pak"
                    : "Couldn't open last directory"
            );
        }
        sound_play_effect(SFX_EXIT);
    } else if (menu->actions.configure && menu->browser.entry && menu->browser.entry->type == ENTRY_TYPE_ROM) {
        menu->load.open_configure = true;
        menu->next_mode = MENU_MODE_LOAD_ROM;
        sound_play_effect(SFX_SETTING);
    } else if ((menu->actions.go_up || menu->actions.go_down) && !menu->actions.go_fast) {
        int pages = ui_components_game_info_page_count(menu->browser.entry);
        if (pages > 1) {
            info_page = (info_page + (menu->actions.go_down ? 1 : pages - 1)) % pages;
            sound_play_effect(SFX_CURSOR);
        }
    } else if (menu->actions.lz_context && !menu->actions.tab_prev && menu->browser.entry) {
        // Z: file options (properties, delete, set default folder)
        ui_components_context_menu_show(&entry_context_menu);
        sound_play_effect(SFX_SETTING);
    } else if (menu->actions.settings) {
        ui_components_context_menu_show(&settings_context_menu);
        sound_play_effect(SFX_SETTING);
    }
}

static void draw (menu_t *menu, surface_t *d) {
    rdpq_attach_clear(d, NULL);

    rdpq_text_printf(NULL, FNT_DEFAULT, CAROUSEL_SELECTED_X, LIBRARY_HEADER_Y, "Library");
    int r_x = VISIBLE_AREA_X1 - ui_components_icon_width(ICON_R);
    ui_components_icon_draw(ICON_R, r_x, LIBRARY_HEADER_Y - 15);
    ui_components_icon_draw(ICON_L, r_x - 6 - ui_components_icon_width(ICON_L), LIBRARY_HEADER_Y - 15);

    ui_components_carousel_draw(menu->browser.directory, menu->browser.list, menu->browser.entries, menu->browser.selected);

    int pages = ui_components_game_info_page_count(menu->browser.entry);
    if (info_page >= pages) {
        info_page = 0;
    }
    ui_components_game_info_draw(menu->browser.directory, menu->browser.entry, &menu->bookkeeping, info_page);
    ui_components_game_info_dots_draw(info_page, pages);

    int x = CAROUSEL_SELECTED_X;
    if (menu->browser.entry) {
        bool is_dir = (menu->browser.entry->type == ENTRY_TYPE_DIR);
        x += ui_components_button_hint_draw(ICON_A, x, LIBRARY_BUTTONS_Y, is_dir ? "Open Folder" : "Play Cartridge") + LIBRARY_HINT_GAP;
        if (menu->browser.entry->type == ENTRY_TYPE_ROM) {
            x += ui_components_button_hint_draw(ICON_C_RIGHT, x, LIBRARY_BUTTONS_Y, "Configure") + LIBRARY_HINT_GAP;
        }
    }
    if (!path_is_root(menu->browser.directory)) {
        ui_components_button_hint_draw(ICON_B, x, LIBRARY_BUTTONS_Y, "Back");
    }

    ui_components_context_menu_draw(&entry_context_menu);

    ui_components_context_menu_draw(&settings_context_menu);

    rdpq_detach_show();
}

void view_browser_init (menu_t *menu) {
    if (!menu->browser.valid) {
        ui_components_context_menu_init(&entry_context_menu);
        ui_components_context_menu_init(&settings_context_menu);
        if (load_directory(menu)) {
            path_free(menu->browser.directory);
            menu->browser.directory = path_init(menu->storage_prefix, "");
            menu_show_error(
                menu,
                directory_entry_limit_exceeded
                    ? "Initial directory is too large for Jumper Pak\nUse an Expansion Pak"
                    : "Error while opening initial directory"
            );
        } else {
            menu->browser.valid = true;
        }
    }

    if (menu->browser.select_file) {
        if (select_file(menu, menu->browser.select_file)) {
            menu->browser.valid = false;
            menu_show_error(
                menu,
                directory_entry_limit_exceeded
                    ? "Target directory is too large for Jumper Pak\nUse an Expansion Pak"
                    : "Error while navigating to file"
            );
        }
        path_free(menu->browser.select_file);
        menu->browser.select_file = NULL;
    }

    if (menu->browser.reload) {
        menu->browser.reload = false;
        if (reload_directory(menu)) {
            menu_show_error(
                menu,
                directory_entry_limit_exceeded
                    ? "Current directory is too large for Jumper Pak\nUse an Expansion Pak"
                    : "Error while reloading current directory"
            );
            menu->browser.valid = false;
        }
    }
}

void view_browser_display (menu_t *menu, surface_t *display) {
    process(menu);

    draw(menu, display);
}
