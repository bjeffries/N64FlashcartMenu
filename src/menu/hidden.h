/**
 * @file hidden.h
 * @brief Games hidden from the Library ("Remove")
 * @ingroup menu
 */

#ifndef HIDDEN_H__
#define HIDDEN_H__

#include <stdbool.h>

#include "path.h"

/**
 * @brief Load the hidden list from a text file (one path per line, without the "sd:" prefix).
 *
 * @param file_path Path of the list file; it is created on the first save.
 */
void hidden_init(const char *file_path);

/** @brief Free the in-memory list. */
void hidden_deinit(void);

/**
 * @brief Check whether a file is hidden.
 *
 * @param path Full path of the ROM or disk.
 */
bool hidden_contains(path_t *path);

/**
 * @brief Hide or unhide a file and save the list.
 *
 * @param path Full path of the ROM or disk.
 * @param hidden true to hide, false to unhide.
 */
void hidden_set(path_t *path, bool hidden);

#endif /* HIDDEN_H__ */
