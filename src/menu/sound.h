/**
 * @file sound.h
 * @brief Menu Sound
 * @ingroup menu
 */

#ifndef SOUND_H__
#define SOUND_H__

#include <stdbool.h>

#define SOUND_SFX_CHANNEL           (0) /**< First Channel of sound effects 0-7 (8 in total) */
#define SOUND_BOOT_CHANNEL          (8) /**< Boot animation cue: stereo, channels 8-9 */
#define SOUND_LOADING_CHANNEL       (9) /**< Wind when a game has loaded (the boot cue is over by then) */
#define SOUND_MP3_PLAYER_CHANNEL    (10) /**< First Channel for MP3 player sound 10-15 (6 in total [surround sound possible]) */


/**
 * @brief Enumeration of available sound effects for menu interactions.
 * 
 * This enumeration defines the different sound effects that can be used
 * for menu interactions.
 */
typedef enum {
    SFX_CURSOR,  /**< Sound effect for cursor movement */
    SFX_ERROR,   /**< Sound effect for error */
    SFX_ENTER,   /**< Sound effect for entering a menu */
    SFX_EXIT,    /**< Sound effect for exiting a menu */
    SFX_SETTING, /**< Sound effect for changing a setting */
} sound_effect_t;

/**
 * @brief Initialize the default sound system.
 * 
 * This function initializes the default sound system, setting up
 * necessary resources and configurations.
 */
void sound_init_default(void);

/**
 * @brief Initialize the sound effects system.
 * 
 * This function initializes the sound effects system, setting up
 * necessary resources and configurations for playing sound effects.
 */
void sound_init_sfx(void);

/**
 * @brief Enable or disable sound effects.
 * 
 * @param enable True to enable sound effects, false to disable.
 */
void sound_use_sfx(bool enable);

/**
 * @brief Play a specified sound effect.
 * 
 * @param sfx The sound effect to play, as defined in sound_effect_t.
 */
void sound_play_effect(sound_effect_t sfx);

/**
 * @brief Deinitialize the sound system.
 * 
 * This function deinitializes the sound system, releasing any resources
 * that were allocated.
 */
void sound_deinit(void);

/**
 * @brief Play the boot animation's sound (rom:/boot/eclipse_boot_hall.wav64).
 *
 * Plays on its own channels regardless of the Sound Effects setting, and closes itself when it
 * ends (in sound_poll).
 */
void sound_play_boot(void);

/**
 * @brief Stop the boot sound if it is still playing.
 *
 * Call before loading a game: it streams from rom:/, which the game overwrites on the SC64.
 */
void sound_stop_boot(void);

/**
 * @brief Read the "game loaded" wind (rom:/loading_wind.wav) into memory, if sound effects are on.
 *
 * Call before loading a game: rom:/ can't be read once loading starts.
 */
void sound_loading_wind_prepare(void);

/** @brief Play the wind prepared by sound_loading_wind_prepare() (does nothing if it wasn't). */
void sound_loading_wind_play(void);

/** @brief Stop and free the wind. */
void sound_loading_wind_free(void);

/**
 * @brief Poll the sound system.
 * 
 * This function polls the sound system, updating its state as necessary.
 */
void sound_poll(void);

#endif /* SOUND_H__ */
