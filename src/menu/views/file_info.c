/**
 * @file file_info.c
 * @brief File properties screen (Z > Show entry properties in the Library)
 * @ingroup view
 */

#include <sys/stat.h>
#include <time.h>
#include "../sound.h"
#include "../ui_components/constants.h"
#include "views.h"

static struct stat st;
static bool have_stat = false;


static const char *name_value (menu_t *menu) {
    return menu->browser.entry->name;
}

static const char *type_value (menu_t *menu) {
    switch (menu->browser.entry->type) {
        case ENTRY_TYPE_DIR: return "Folder";
        case ENTRY_TYPE_ROM: return "N64 game";
        default: return "File";
    }
}

static const char *size_value (menu_t *menu) {
    static char buffer[32];
    if (!have_stat || S_ISDIR(st.st_mode)) {
        return "-";
    }
    if (st.st_size >= (1024 * 1024)) {
        snprintf(buffer, sizeof(buffer), "%.1f MB", st.st_size / (1024.0f * 1024.0f));
    } else {
        snprintf(buffer, sizeof(buffer), "%ld KB", (long) (st.st_size / 1024));
    }
    return buffer;
}

static const char *modified_value (menu_t *menu) {
    static char buffer[32];
    struct tm *tm = have_stat ? gmtime(&st.st_mtime) : NULL;
    // FAT timestamps start in 1980; anything that early means "no real date".
    if (!tm || tm->tm_year + 1900 < 1990) {
        return "-";
    }
    strftime(buffer, sizeof(buffer), "%b %d, %Y %H:%M", tm);
    return buffer;
}

static const char *attributes_value (menu_t *menu) {
    static char buffer[48];
    if (!have_stat) {
        return "-";
    }
    snprintf(buffer, sizeof(buffer), "%s%s",
        FAT_ATTR_IS_RDO(&st) ? "Read-only" : "Writable",
        FAT_ATTR_IS_HID(&st) ? ", Hidden" : "");
    return buffer;
}

static option_t options[] = {
    { .label = "Name", .type = OPTION_INFO, .value = name_value },
    { .label = "Type", .type = OPTION_INFO, .value = type_value },
    { .label = "Size", .type = OPTION_INFO, .value = size_value },
    { .label = "Modified", .type = OPTION_INFO, .value = modified_value },
    { .label = "Attributes", .type = OPTION_INFO, .value = attributes_value },
};

static option_list_t list = {
    .options = options,
    .count = sizeof(options) / sizeof(options[0]),
};


static void process (menu_t *menu) {
    if (menu->actions.back) {
        sound_play_effect(SFX_EXIT);
        menu->next_mode = MENU_MODE_BROWSER;
    }
}

static void draw (menu_t *menu, surface_t *d) {
    rdpq_attach_clear(d, NULL);

    ui_components_option_screen_draw(menu, "Properties", &list);

    rdpq_detach_show();
}


void view_file_info_init (menu_t *menu) {
    path_t *path = path_clone_push(menu->browser.directory, menu->browser.entry->name);
    have_stat = (stat(path_get(path), &st) == 0);
    path_free(path);

    if (!have_stat) {
        menu_show_error(menu, "Couldn't read this file's information");
    }
    ui_components_option_list_init(&list);
}

void view_file_info_display (menu_t *menu, surface_t *display) {
    process(menu);
    draw(menu, display);
}
