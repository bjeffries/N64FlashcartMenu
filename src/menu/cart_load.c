/**
 * @file cart_load.c
 * @brief Cart loading functions
 * @ingroup menu
 */

#include <string.h>
#include <libdragon.h>
#include "cart_load.h"
#include "ini_parser.h"
#include "path.h"
#include "utils/fs.h"
#include "utils/utils.h"

#ifndef DDIPL_LOCATION
#define DDIPL_LOCATION          "/menu/64ddipl"
#endif

/**
 * @brief Check if the 64DD is connected.
 * 
 * @return true if the 64DD is connected, false otherwise.
 */
bool is_64dd_connected (void) {
    bool is_64dd_io_present = ((io_read(0x05000540) & 0x0000FFFF) == 0x0000);
    bool is_64dd_ipl_present = (io_read(0x06001010) == 0x2129FFF8);
    return (is_64dd_io_present || is_64dd_ipl_present);
}

/**
 * @brief Create the saves subdirectory.
 * 
 * @param path Pointer to the path structure.
 * @return true if an error occurred, false otherwise.
 */
static bool create_saves_subdirectory (path_t *path) {
    path_t *save_folder_path = path_clone(path);
    path_pop(save_folder_path);
    path_push(save_folder_path, SAVE_DIRECTORY_NAME);
    bool error = directory_create(path_get(save_folder_path));
    path_free(save_folder_path);
    return error;
}

/**
 * @brief Convert the ROM save type to the flashcart save type.
 * 
 * @param save_type The ROM save type.
 * @return flashcart_save_type_t The flashcart save type.
 */
static flashcart_save_type_t convert_save_type (rom_save_type_t save_type) {
    switch (save_type) {
        case SAVE_TYPE_EEPROM_4KBIT: return FLASHCART_SAVE_TYPE_EEPROM_4KBIT;
        case SAVE_TYPE_EEPROM_16KBIT: return FLASHCART_SAVE_TYPE_EEPROM_16KBIT;
        case SAVE_TYPE_SRAM_256KBIT: return FLASHCART_SAVE_TYPE_SRAM_256KBIT;
        case SAVE_TYPE_SRAM_BANKED: return FLASHCART_SAVE_TYPE_SRAM_BANKED;
        case SAVE_TYPE_SRAM_1MBIT: return FLASHCART_SAVE_TYPE_SRAM_1MBIT;
        case SAVE_TYPE_FLASHRAM_1MBIT: return FLASHCART_SAVE_TYPE_FLASHRAM_1MBIT;
        case SAVE_TYPE_FLASHRAM_PKST2: return FLASHCART_SAVE_TYPE_FLASHRAM_PKST2;
        default: return FLASHCART_SAVE_TYPE_NONE;
    }
}

/**
 * @brief Convert the cart load error code to a human-readable message.
 * 
 * @param err The cart load error code.
 * @return char* The error message.
 */
char *cart_load_convert_error_message (cart_load_err_t err) {
    switch (err) {
        case CART_LOAD_OK: return "Cart load OK";
        case CART_LOAD_ERR_ROM_LOAD_FAIL: return "Error occured during ROM loading";
        case CART_LOAD_ERR_SAVE_LOAD_FAIL: return "Error occured during save loading";
        case CART_LOAD_ERR_BOOT_MODE_FAIL: return "Error occured during boot mode setting";
        case CART_LOAD_ERR_64DD_PRESENT: return "64DD accessory is connected to the N64";
        case CART_LOAD_ERR_64DD_IPL_NOT_FOUND: return "Required 64DD IPL file was not found";
        case CART_LOAD_ERR_64DD_IPL_LOAD_FAIL: return "Error occurred during 64DD IPL loading";
        case CART_LOAD_ERR_64DD_DISK_LOAD_FAIL: return "Error occurred during 64DD disk loading";
        case CART_LOAD_ERR_CREATE_SAVES_SUBDIR_FAIL: return "Couldn't create saves subdirectory";
        case CART_LOAD_ERR_EXP_PAK_NOT_FOUND: return "Mandatory Expansion Pak accessory was not found";
        case CART_LOAD_ERR_FUNCTION_NOT_SUPPORTED: return "Your flashcart doesn't support required functionality";
        default: return "Unknown error [CART_LOAD]";
    }
}

/**
 * @brief Load an N64 ROM and its save file.
 * 
 * @param menu Pointer to the menu structure.
 * @param progress Progress callback function.
 * @return cart_load_err_t Error code.
 */
cart_load_err_t cart_load_n64_rom_and_save (menu_t *menu, flashcart_progress_callback_t progress, flashcart_progress_callback_t save_progress) {
    path_t *path = path_clone(menu->load.rom_path);

    bool byte_swap = (menu->load.rom_info.endianness == ENDIANNESS_BYTE_SWAP);
    flashcart_save_type_t save_type = convert_save_type(rom_info_get_save_type(&menu->load.rom_info));

    menu->flashcart_err = flashcart_load_rom(path_get(path), byte_swap, progress);
    if (menu->flashcart_err != FLASHCART_OK) {
        path_free(path);
        return CART_LOAD_ERR_ROM_LOAD_FAIL;
    }

    path_ext_replace(path, "sav");
    if (menu->settings.use_saves_folder) {
        if ((save_type != FLASHCART_SAVE_TYPE_NONE) && create_saves_subdirectory(path)) {
            path_free(path);
            return CART_LOAD_ERR_CREATE_SAVES_SUBDIR_FAIL;
        }
        path_push_subdir(path, SAVE_DIRECTORY_NAME);
    }

    if (save_progress && save_type != FLASHCART_SAVE_TYPE_NONE && !file_exists(path_get(path))) {
        save_progress(1.0f);
    }

    menu->flashcart_err = flashcart_load_save(path_get(path), save_type);
    if (menu->flashcart_err != FLASHCART_OK) {
        path_free(path);
        return CART_LOAD_ERR_SAVE_LOAD_FAIL;
    }

#ifndef FEATURE_AUTOLOAD_ROM_ENABLED
    if (menu->settings.rom_fast_reboot_enabled) {
        if (!flashcart_has_feature(FLASHCART_FEATURE_ROM_REBOOT_FAST)) {
            return CART_LOAD_ERR_FUNCTION_NOT_SUPPORTED;
        }
        menu->flashcart_err = flashcart_set_next_boot_mode(FLASHCART_REBOOT_MODE_ROM);
        if (menu->flashcart_err != FLASHCART_OK) {
            path_free(path);
            return CART_LOAD_ERR_BOOT_MODE_FAIL;
        }
    }
#endif

    path_free(path);

    return CART_LOAD_OK;
}

/**
 * @brief Load the 64DD IPL and disk.
 * 
 * @param menu Pointer to the menu structure.
 * @param progress Progress callback function.
 * @return cart_load_err_t Error code.
 */
cart_load_err_t cart_load_64dd_ipl_and_disks (menu_t *menu, flashcart_progress_callback_t progress) {
    if (!flashcart_has_feature(FLASHCART_FEATURE_64DD)) {
        return CART_LOAD_ERR_FUNCTION_NOT_SUPPORTED;
    }

    if (is_64dd_connected()) {
        return CART_LOAD_ERR_64DD_PRESENT;
    }

    if (!is_memory_expanded()) {
        return CART_LOAD_ERR_EXP_PAK_NOT_FOUND;
    }

    path_t *path = path_init(menu->storage_prefix, DDIPL_LOCATION);
    flashcart_disk_parameters_t disk_parameters;

    disk_parameters.development_drive = (menu->load.disk_slots.primary.disk_info.region == DISK_REGION_DEVELOPMENT);
    disk_parameters.disk_type = menu->load.disk_slots.primary.disk_info.disk_type;
    memcpy(disk_parameters.bad_system_area_lbas, menu->load.disk_slots.primary.disk_info.bad_system_area_lbas, sizeof(disk_parameters.bad_system_area_lbas));
    memcpy(disk_parameters.defect_tracks, menu->load.disk_slots.primary.disk_info.defect_tracks, sizeof(disk_parameters.defect_tracks));

    switch (menu->load.disk_slots.primary.disk_info.region) {
        case DISK_REGION_DEVELOPMENT:
            path_push(path, "NDXJ0.n64");
            break;
        case DISK_REGION_JAPANESE:
            path_push(path, "NDDJ2.n64");
            break;
        case DISK_REGION_USA:
            path_push(path, "NDDE0.n64");
            break;
    }

    if (!file_exists(path_get(path))) {
        path_free(path);
        return CART_LOAD_ERR_64DD_IPL_NOT_FOUND;
    }

    menu->flashcart_err = flashcart_load_64dd_ipl(path_get(path), progress);
    if (menu->flashcart_err != FLASHCART_OK) {
        path_free(path);
        return CART_LOAD_ERR_64DD_IPL_LOAD_FAIL;
    }

    path_free(path);

    int swap_disk_count = 0;
    char *swap_disk_paths[3];
    for (int i = 0; i < 3; i++) {
        if (menu->load.disk_slots.swap_slot[i].disk_path) {
            swap_disk_paths[swap_disk_count++] = path_get(menu->load.disk_slots.swap_slot[i].disk_path);
        }
    }

    if (swap_disk_count > 0) {
        menu->flashcart_err = flashcart_load_64dd_disks(path_get(menu->load.disk_slots.primary.disk_path), &disk_parameters, swap_disk_paths, swap_disk_count);
    } else {
        menu->flashcart_err = flashcart_load_64dd_disk(path_get(menu->load.disk_slots.primary.disk_path), &disk_parameters);
    }
    if (menu->flashcart_err != FLASHCART_OK) {
        return CART_LOAD_ERR_64DD_DISK_LOAD_FAIL;
    }

    return CART_LOAD_OK;
}
