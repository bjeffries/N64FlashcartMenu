/**
 * @file cart_load.h
 * @brief ROM/save loading functions
 * @ingroup menu
 */

#ifndef CART_LOAD_H__
#define CART_LOAD_H__

#include "flashcart/flashcart.h"
#include "menu_state.h"
#include "rom_info.h"

#ifndef SAVE_DIRECTORY_NAME
#define SAVE_DIRECTORY_NAME "saves"
#endif

/** @brief Cart load state enumeration. */
typedef enum {
    /** @brief Returned no error. */
    CART_LOAD_OK,
    /** @brief Failed to load the ROM correctly. */
    CART_LOAD_ERR_ROM_LOAD_FAIL,
    /** @brief Failed to load the save correctly. */
    CART_LOAD_ERR_SAVE_LOAD_FAIL,
    /** @brief Failed to set the next boot mode. */
    CART_LOAD_ERR_BOOT_MODE_FAIL,
    /** @brief Failed to create the save sub-directory. */
    CART_LOAD_ERR_CREATE_SAVES_SUBDIR_FAIL,
    /** @brief There was not enough system memory available (expected an Expansion PAK). */
    CART_LOAD_ERR_EXP_PAK_NOT_FOUND,
    /** @brief An unexpected response. */
    CART_LOAD_ERR_FUNCTION_NOT_SUPPORTED,
} cart_load_err_t;

/**
 * @brief Convert a cart load error code to a human-readable error message.
 * 
 * @param err The cart load error code.
 * @return char* The human-readable error message.
 */
char *cart_load_convert_error_message(cart_load_err_t err);

/**
 * @brief Load an N64 ROM and its save data.
 * 
 * @param menu Pointer to the menu structure.
 * @param progress Callback function for ROM load progress updates.
 * @param save_progress Callback invoked once when a new save file is being created, or NULL.
 * @return cart_load_err_t Error code.
 */
cart_load_err_t cart_load_n64_rom_and_save(menu_t *menu, flashcart_progress_callback_t progress, flashcart_progress_callback_t save_progress);

#endif /* CART_LOAD_H__ */
