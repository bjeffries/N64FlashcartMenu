/**
 * @file bookkeeping.h
 * @brief Bookkeeping of loaded ROMs.
 * @ingroup menu 
 */

#ifndef BOOKKEEPING_H__
#define BOOKKEEPING_H__

#include <time.h>

#include "path.h"

#define FAVORITES_COUNT 32 /**< Maximum number of favorite items */
#define HISTORY_COUNT 32 /**< Maximum number of history items (also how many games keep a Last Played date) */

/** @brief Bookkeeping item types enumeration */
typedef enum {
    BOOKKEEPING_TYPE_EMPTY, /**< Empty item */
    BOOKKEEPING_TYPE_ROM, /**< ROM item */
    BOOKKEEPING_TYPE_DISK, /**< Disk item */
} bookkeeping_item_types_t;

/** @brief Bookkeeping item structure */
typedef struct {
    path_t *primary_path; /**< Primary path */
    path_t *secondary_path; /**< Secondary path */
    bookkeeping_item_types_t bookkeeping_type; /**< Bookkeeping item type */
    time_t last_played; /**< When the item was last launched (0 if unknown); only used by history */
} bookkeeping_item_t;

/** @brief ROM bookkeeping structure */
typedef struct {
    bookkeeping_item_t history_items[HISTORY_COUNT]; /**< History items */
    bookkeeping_item_t favorite_items[FAVORITES_COUNT]; /**< Favorite items */
} bookkeeping_t;

/**
 * @brief Initialize ROM bookkeeping path.
 * 
 * @param path The path to initialize.
 */
void bookkeeping_init(char *path);

/**
 * @brief Load ROM bookkeeping.
 * 
 * @param history Pointer to the bookkeeping structure to load.
 */
void bookkeeping_load(bookkeeping_t *history);

/**
 * @brief Save ROM bookkeeping.
 * 
 * @param history Pointer to the bookkeeping structure to save.
 */
void bookkeeping_save(bookkeeping_t *history);

/**
 * @brief Add a ROM to the history.
 * 
 * @param bookkeeping The bookkeeping structure.
 * @param primary_path The primary path of the ROM.
 * @param secondary_path The secondary path of the ROM.
 * @param type The type of the bookkeeping item.
 */
void bookkeeping_history_add(bookkeeping_t *bookkeeping, path_t *primary_path, path_t *secondary_path, bookkeeping_item_types_t type);

/**
 * @brief Find when a file was last launched, from the history list.
 *
 * @param bookkeeping Pointer to the bookkeeping structure.
 * @param path Full path of the ROM or disk.
 * @return Launch time, or 0 if the file isn't in the history (or was launched without a clock).
 */
time_t bookkeeping_history_last_played(bookkeeping_t *bookkeeping, path_t *path);

/**
 * @brief Find a file in the favorites.
 *
 * @param bookkeeping Pointer to the bookkeeping structure.
 * @param path Full path of the ROM or disk.
 * @return Its favorites slot, or -1 if it isn't a favorite.
 */
int bookkeeping_favorite_find(bookkeeping_t *bookkeeping, path_t *path);

/**
 * @brief Add a ROM to the favorites.
 * 
 * @param bookkeeping The bookkeeping structure.
 * @param primary_path The primary path of the ROM.
 * @param secondary_path The secondary path of the ROM.
 * @param type The type of the bookkeeping item.
 */
void bookkeeping_favorite_add(bookkeeping_t *bookkeeping, path_t *primary_path, path_t *secondary_path, bookkeeping_item_types_t type);

/**
 * @brief Remove a ROM from the favorites.
 * 
 * @param bookkeeping The bookkeeping structure.
 * @param selection The index of the favorite item to remove.
 */
void bookkeeping_favorite_remove(bookkeeping_t *bookkeeping, int selection);

#endif /* BOOKKEEPING_H__ */
