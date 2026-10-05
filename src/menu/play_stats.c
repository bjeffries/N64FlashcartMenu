/**
 * @file play_stats.c
 * @brief How many times each game has been played, and when it was last played
 * @ingroup menu
 *
 * Kept for every game ever played (the History list only keeps the most recent few), in a plain
 * text file with one game per line: "count<TAB>last played (Unix time)<TAB>path", the path
 * without its storage prefix (e.g. "/N64/Game.z64"). Paths are compared case-insensitively,
 * like the FAT filesystem.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <libdragon.h>

#include "play_stats.h"
#include "utils/fs.h"

#define MAX_LINE    (600)

typedef struct {
    char *path;
    uint32_t count;
    time_t last_played;
} play_stat_t;

static char *file_path = NULL;
static play_stat_t *stats = NULL;
static int count = 0;
static int capacity = 0;


/** @brief The path without its storage prefix ("sd:/N64/Game.z64" -> "/N64/Game.z64"). */
static const char *strip_prefix (const char *path) {
    const char *separator = strstr(path, ":/");
    return separator ? separator + 1 : path;
}

static play_stat_t *find (const char *path) {
    path = strip_prefix(path);
    for (int i = 0; i < count; i++) {
        if (strcasecmp(stats[i].path, path) == 0) {
            return &stats[i];
        }
    }
    return NULL;
}

static play_stat_t *add (const char *path) {
    if (count == capacity) {
        int new_capacity = capacity ? capacity * 2 : 64;
        play_stat_t *grown = realloc(stats, new_capacity * sizeof(play_stat_t));
        if (!grown) {
            return NULL;
        }
        stats = grown;
        capacity = new_capacity;
    }
    play_stat_t *stat = &stats[count];
    stat->path = strdup(strip_prefix(path));
    if (!stat->path) {
        return NULL;
    }
    stat->count = 0;
    stat->last_played = 0;
    count++;
    return stat;
}

static void save (void) {
    if (!file_path) {
        return;
    }
    FILE *f = fopen(file_path, "w");
    if (!f) {
        debugf("[PLAY_STATS] Failed to save %s\n", file_path);
        return;
    }
    for (int i = 0; i < count; i++) {
        fprintf(f, "%lu\t%lld\t%s\n", (unsigned long) stats[i].count, (long long) stats[i].last_played, stats[i].path);
    }
    fclose(f);
}

static void load (void) {
    FILE *f = fopen(file_path, "r");
    if (!f) {
        return;
    }
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *end;
        unsigned long plays = strtoul(line, &end, 10);
        if (*end != '\t') {
            continue;
        }
        long long last = strtoll(end + 1, &end, 10);
        if (*end != '\t' || end[1] == '\0') {
            continue;
        }
        play_stat_t *stat = find(end + 1);
        if (!stat) {
            stat = add(end + 1);
        }
        if (stat) {
            stat->count = (uint32_t) plays;
            stat->last_played = (time_t) last;
        }
    }
    fclose(f);
}


void play_stats_init (const char *path, bookkeeping_t *bookkeeping) {
    play_stats_deinit();
    file_path = strdup(path);

    if (file_exists(file_path)) {
        load();
        return;
    }

    // First run: start from the History list, each game played once.
    for (int i = 0; i < HISTORY_COUNT; i++) {
        bookkeeping_item_t *item = &bookkeeping->history_items[i];
        if (item->bookkeeping_type != BOOKKEEPING_TYPE_ROM || !path_has_value(item->primary_path)) {
            continue;
        }
        const char *game = path_get(item->primary_path);
        if (find(game)) {
            continue;
        }
        play_stat_t *stat = add(game);
        if (stat) {
            stat->count = 1;
            stat->last_played = item->last_played;
        }
    }
    save();
}

void play_stats_deinit (void) {
    for (int i = 0; i < count; i++) {
        free(stats[i].path);
    }
    free(stats);
    stats = NULL;
    count = 0;
    capacity = 0;
    free(file_path);
    file_path = NULL;
}

void play_stats_record (path_t *path) {
    play_stat_t *stat = find(path_get(path));
    if (!stat) {
        stat = add(path_get(path));
    }
    if (!stat) {
        return;
    }
    time_t now = time(NULL);
    stat->count++;
    stat->last_played = (now > 0) ? now : 0;
    save();
}

uint32_t play_stats_count (const char *path) {
    play_stat_t *stat = find(path);
    return stat ? stat->count : 0;
}

time_t play_stats_last_played (const char *path) {
    play_stat_t *stat = find(path);
    return stat ? stat->last_played : 0;
}
