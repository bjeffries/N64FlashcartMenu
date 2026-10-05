/**
 * @file play_stats.h
 * @brief How many times each game has been played, and when it was last played
 * @ingroup menu
 */

#ifndef PLAY_STATS_H__
#define PLAY_STATS_H__

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "bookkeeping.h"
#include "path.h"

/**
 * @brief Load the play counts from file_path (menu/play_stats.txt). The first time, it's started
 *        from the History list: each game in it counted once, with its Last Played date.
 */
void play_stats_init(const char *file_path, bookkeeping_t *bookkeeping);

/** @brief Free the play counts. */
void play_stats_deinit(void);

/** @brief Count a game as played now, and save. */
void play_stats_record(path_t *path);

/** @brief How many times a game has been played (0: never). */
uint32_t play_stats_count(const char *path);

/** @brief When a game was last played (0: never, or unknown). */
time_t play_stats_last_played(const char *path);

#endif
