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
 * @return true if the ROM could be read.
 */
bool labels_rom_id(const char *rom_path, uint32_t *id);

/**
 * @brief Load the label image for a label ID.
 *
 * @param id Label ID from #labels_rom_id.
 * @return Newly allocated RGBA16 surface (free with #labels_free), or NULL if there is no label.
 */
surface_t *labels_load(uint32_t id);

/**
 * @brief Free a label surface once the RDP has finished drawing it.
 *
 * @param label Surface returned by #labels_load (may be NULL).
 */
void labels_free(surface_t *label);

#endif /* LABELS_H__ */
