/**
 * @file labels.h
 * @brief Cartridge label images from an Analogue 3D labels.db file
 * @ingroup menu
 */

#ifndef LABELS_H__
#define LABELS_H__

#include <stdbool.h>
#include <stdint.h>
#include <libdragon.h>

/** @brief Width of a label image (pixels). */
#define LABEL_WIDTH     (74)
/** @brief Height of a label image (pixels). */
#define LABEL_HEIGHT    (86)

/**
 * @brief Open a labels.db file and load its ID table.
 *
 * @param db_path Path to labels.db.
 * @return true if the database was opened.
 */
bool labels_init(const char *db_path);

/** @brief Release the ID table. */
void labels_deinit(void);

/**
 * @brief Compute the label ID of a ROM: CRC32 of its first 8 KiB in big-endian byte order.
 *
 * @param rom_path Path to the ROM file.
 * @param id Output label ID.
 * @param game_code Optional output: the 4-character game code from the header (may be NULL).
 * @return true if the ROM could be read.
 */
bool labels_rom_id(const char *rom_path, uint32_t *id, char game_code[4]);

/**
 * @brief Read the label image for a label ID from labels.db once and draw it into existing RGBA16
 *        surfaces at their sizes (at most #LABEL_WIDTH x #LABEL_HEIGHT). Allocates nothing.
 *
 * @param id Label ID from #labels_rom_id.
 * @param large Surface for the first size.
 * @param small Surface for a second size, or NULL.
 * @return false if there is no label for this ID (the surfaces are left as they were).
 */
bool labels_load_into(uint32_t id, surface_t *large, surface_t *small);

/**
 * @brief Free a label surface once the RDP has finished drawing it.
 *
 * @param label Surface returned by #labels_load (may be NULL).
 */
void labels_free(surface_t *label);

#endif /* LABELS_H__ */
