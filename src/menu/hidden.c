/**
 * @file hidden.c
 * @brief Games hidden from the Library ("Remove")
 * @ingroup menu
 *
 * The list is a plain text file with one path per line (e.g. "/N64/Game.z64"), so it can also
 * be edited on a computer. Paths are compared case-insensitively, like the FAT filesystem.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <libdragon.h>

#include "hidden.h"
#include "utils/fs.h"

#define MAX_LINE    (512)

static char *list_path = NULL;
static char **entries = NULL;
static int count = 0;
static int capacity = 0;


static void add_entry (const char *path) {
    if (count == capacity) {
        int new_capacity = capacity ? capacity * 2 : 16;
        char **grown = realloc(entries, new_capacity * sizeof(char *));
        if (!grown) {
            return;
        }
        entries = grown;
        capacity = new_capacity;
    }
    char *copy = strdup(path);
    if (copy) {
        entries[count++] = copy;
    }
}

static int find_entry (const char *path) {
    for (int i = 0; i < count; i++) {
        if (strcasecmp(entries[i], path) == 0) {
            return i;
        }
    }
    return -1;
}

static void save (void) {
    FILE *f = list_path ? fopen(list_path, "w") : NULL;
    if (!f) {
        debugf("[HIDDEN] Failed to save %s\n", list_path ? list_path : "(no path)");
        return;
    }
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s\n", entries[i]);
    }
    fclose(f);
}

void hidden_init (const char *file_path) {
    hidden_deinit();
    list_path = strdup(file_path);

    FILE *f = fopen(file_path, "r");
    if (!f) {
        return;
    }
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] != '\0' && find_entry(line) < 0) {
            add_entry(line);
        }
    }
    fclose(f);
    debugf("[HIDDEN] %d hidden games\n", count);
}

void hidden_deinit (void) {
    for (int i = 0; i < count; i++) {
        free(entries[i]);
    }
    free(entries);
    entries = NULL;
    count = 0;
    capacity = 0;
    free(list_path);
    list_path = NULL;
}

bool hidden_contains (path_t *path) {
    return count > 0 && find_entry(strip_fs_prefix(path_get(path))) >= 0;
}

void hidden_set (path_t *path, bool hidden) {
    const char *stripped = strip_fs_prefix(path_get(path));
    int index = find_entry(stripped);

    if (hidden && index < 0) {
        add_entry(stripped);
    } else if (!hidden && index >= 0) {
        free(entries[index]);
        entries[index] = entries[--count];
    } else {
        return;
    }
    save();
}
