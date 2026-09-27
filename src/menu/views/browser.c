#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "../cart_load.h"
#include "../fonts.h"
#include "../hidden.h"
#include "../ui_components/constants.h"
#include "utils/fs.h"
#include "views.h"
#include "../sound.h"

static const char *cheat_extensions[] = {"cht", "cheats", "datel", "gameshark", NULL};
static const char *image_extensions[] = { "png", NULL };
static const char *n64_rom_extensions[] = { "z64", "n64", "v64", "rom", NULL };
static const char *save_extensions[] = { "sav", "eep", "sra", "srm", "fla", NULL };
static const char *text_extensions[] = { "txt", "ini", "yml", "yaml", NULL };
static const char *rom_meta_extensions[] = { "meta", "metadata", NULL };

// Fixed cap keeps memory use predictable on 4MB systems when scanning huge folders.
#define DIRECTORY_MAX_ENTRIES_JUMPER_PAK 1024

static bool directory_entry_limit_exceeded = false;
static int info_page = 0;
static bool confirm_hide = false;

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
            if (entry->type != ENTRY_TYPE_DIR && entry->type != ENTRY_TYPE_ROM) {
                free(entry->name);
                result = dir_findnext(path_get(path), &info);
                continue;
            }

            // Games removed from the Library ("Remove") are only listed when the setting asks for them.
            entry->hidden = false;
            if (entry->type != ENTRY_TYPE_DIR) {
                path_push(path, entry->name);
                entry->hidden = hidden_contains(path);
                path_pop(path);
            }
            if (entry->hidden && !menu->settings.show_hidden_games) {
                free(entry->name);
                result = dir_findnext(path_get(path), &info);
                continue;
            }

            entry->size = info.d_size;
            entry->index = menu->browser.entries;
            menu->browser.entries++;
        }

        sound_poll();   // keep audio (the boot sound) flowing through long SD scans

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
    // An empty folder has no selection (-1); once it has entries again, select the first.
    if (menu->browser.selected < 0 && menu->browser.entries > 0) {
        menu->browser.selected = 0;
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

/** @brief Whether the selected folder is the Library's start folder (Menu Settings > Start Folder). */
static bool selected_folder_is_default (menu_t *menu) {
    path_t *path = path_clone_push(menu->browser.directory, menu->browser.entry->name);
    bool is_default = (strcmp(strip_fs_prefix(path_get(path)), menu->settings.default_directory) == 0);
    path_free(path);
    return is_default;
}

/** @brief Make the selected folder the one the Library opens in. */
static void set_selected_folder_default (menu_t *menu) {
    path_t *path = path_clone_push(menu->browser.directory, menu->browser.entry->name);
    free(menu->settings.default_directory);
    menu->settings.default_directory = strdup(strip_fs_prefix(path_get(path)));
    path_free(path);
    settings_save(&menu->settings);
}

/** @brief Hide or unhide the selected game, then refresh the list if it just disappeared from it. */
static void set_selected_hidden (menu_t *menu, bool hide) {
    path_t *path = path_clone_push(menu->browser.directory, menu->browser.entry->name);
    hidden_set(path, hide);
    path_free(path);
    if (menu->settings.show_hidden_games) {
        menu->browser.entry->hidden = hide;
    } else if (reload_directory(menu)) {
        menu->browser.valid = false;
        menu_show_error(menu, "Couldn't refresh directory contents");
    }
    sound_play_effect(SFX_SETTING);
}

static void process (menu_t *menu) {
    if (confirm_hide) {
        if (menu->actions.enter) {
            confirm_hide = false;
            set_selected_hidden(menu, true);
        } else if (menu->actions.back) {
            confirm_hide = false;
            sound_play_effect(SFX_EXIT);
        }
        return;
    }

    if (ui_components_tab_process(menu, TAB_LIBRARY)) {
        return;
    }

    // Circular: left from the first game goes to the last. A long hold pages by letter.
    int32_t selected = ui_components_carousel_scroll(menu, menu->browser.list, menu->browser.entries, menu->browser.selected, true);
    if (selected != menu->browser.selected) {
        menu->browser.selected = selected;
        menu->browser.entry = &menu->browser.list[selected];
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
            case ENTRY_TYPE_IMAGE:
                menu->next_mode = MENU_MODE_IMAGE_VIEWER;
                break;
            case ENTRY_TYPE_ROM:
                menu->load.return_mode = MENU_MODE_BROWSER;
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
        menu->load.return_mode = MENU_MODE_BROWSER;
        menu->load.open_configure = true;
        menu->next_mode = MENU_MODE_LOAD_ROM;
        sound_play_effect(SFX_SETTING);
    } else if (menu->actions.favorite && menu->browser.entry && menu->browser.entry->type != ENTRY_TYPE_DIR) {
        path_t *path = path_clone_push(menu->browser.directory, menu->browser.entry->name);
        int slot = bookkeeping_favorite_find(&menu->bookkeeping, path);
        if (slot >= 0) {
            bookkeeping_favorite_remove(&menu->bookkeeping, slot);
        } else {
            bookkeeping_favorite_add(&menu->bookkeeping, path, NULL, BOOKKEEPING_TYPE_ROM);
        }
        path_free(path);
        sound_play_effect(SFX_SETTING);
    } else if (menu->actions.favorite && menu->browser.entry && menu->browser.entry->type == ENTRY_TYPE_DIR) {
        // C-Left on a folder: open the Library there from now on (reset in Menu Settings).
        if (!selected_folder_is_default(menu)) {
            set_selected_folder_default(menu);
            sound_play_effect(SFX_SETTING);
        }
    } else if (menu->actions.remove && menu->browser.entry && menu->browser.entry->type != ENTRY_TYPE_DIR) {
        if (menu->browser.entry->hidden) {
            set_selected_hidden(menu, false);       // unhiding is harmless: no confirmation
        } else {
            confirm_hide = true;
            sound_play_effect(SFX_SETTING);
        }
    } else if ((menu->actions.go_up || menu->actions.go_down) && !menu->actions.go_fast) {
        int pages = ui_components_game_info_page_count(menu->browser.entry);
        // On the About page, up / down scroll the description first and change page at its ends.
        if (ui_components_game_info_scroll(menu->actions.go_down ? 1 : -1)) {
            sound_play_effect(SFX_CURSOR);
        } else if (pages > 1) {
            info_page = (info_page + (menu->actions.go_down ? 1 : pages - 1)) % pages;
            sound_play_effect(SFX_CURSOR);
        }
    }
}

static void draw (menu_t *menu, surface_t *d) {
    rdpq_attach_clear(d, NULL);

    ui_components_tab_header_draw(TAB_LIBRARY);

    bool favorite = false;
    if (menu->browser.entry && menu->browser.entry->type != ENTRY_TYPE_DIR) {
        path_t *path = path_clone_push(menu->browser.directory, menu->browser.entry->name);
        favorite = bookkeeping_favorite_find(&menu->bookkeeping, path) >= 0;
        path_free(path);
    }

    ui_components_carousel_draw(menu->browser.directory, menu->browser.list, menu->browser.entries, menu->browser.selected, favorite);

    int pages = ui_components_game_info_page_count(menu->browser.entry);
    if (info_page >= pages) {
        info_page = 0;
    }
    ui_components_game_info_draw(menu->browser.directory, menu->browser.entry, &menu->bookkeeping, info_page);
    ui_components_game_info_dots_draw(info_page, pages);
    ui_components_letter_indicator_draw();

    // Games show Play / Config / Favorite / Hide; folders show Open / Set to Default / Back.
    int x = GAME_INFO_VALUE_X;
    entry_t *entry = menu->browser.entry;
    bool is_game = entry && entry->type != ENTRY_TYPE_DIR;
    if (entry) {
        x += ui_components_button_hint_draw(ICON_A, x, LIBRARY_BUTTONS_Y, is_game ? "Play" : "Open Folder") + LIBRARY_HINT_GAP;
        if (entry->type == ENTRY_TYPE_ROM) {
            x += ui_components_button_hint_draw(ICON_C_RIGHT, x, LIBRARY_BUTTONS_Y, "Config") + LIBRARY_HINT_GAP;
        }
        if (is_game) {
            x += ui_components_button_hint_draw(ICON_C_LEFT, x, LIBRARY_BUTTONS_Y, favorite ? "Unfavorite" : "Favorite") + LIBRARY_HINT_GAP;
            ui_components_button_hint_draw(ICON_C_UP, x, LIBRARY_BUTTONS_Y, entry->hidden ? "Unhide" : "Hide");
        } else if (entry->type == ENTRY_TYPE_DIR && !selected_folder_is_default(menu)) {
            x += ui_components_button_hint_draw(ICON_C_LEFT, x, LIBRARY_BUTTONS_Y, "Set to Default") + LIBRARY_HINT_GAP;
        }
    }
    if (!is_game && !path_is_root(menu->browser.directory)) {
        ui_components_button_hint_draw(ICON_B, x, LIBRARY_BUTTONS_Y, "Back");
    }


    if (confirm_hide && menu->browser.entry) {
        char title[128];
        ui_components_carousel_entry_title(menu->browser.entry, menu->browser.selected, title, sizeof(title));
        ui_components_messagebox_draw(
            "Hide %s?\n\n"
            "It stays on your SD card. To bring it back, turn on\n"
            "Show Hidden Games in Menu Settings.\n\n"
            "A: Hide    B: Cancel",
            title
        );
    }

    rdpq_detach_show();
}

void view_browser_init (menu_t *menu) {
    confirm_hide = false;
    ui_components_carousel_scroll_reset();

    // Favorites and History share the carousel, so its cached labels may belong to another list.
    ui_components_carousel_invalidate();

    if (!menu->browser.valid) {
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
