/**
 * @file sound.c
 * @brief Sound component implementation
 * @ingroup ui_components
 */

#include <stdbool.h>
#include <libdragon.h>
#include "sound.h"

#define DEFAULT_FREQUENCY   (44100)
#define NUM_BUFFERS         (4)
#define NUM_CHANNELS        (16)

static wav64_t sfx_cursor, sfx_error, sfx_enter, sfx_exit, sfx_setting;

static bool sound_initialized = false;
static bool sfx_enabled = false;
static bool sfx_opened = false;

static wav64_t boot_sound;
static bool boot_sound_opened = false;

// "Game loaded" wind: 16-bit mono PCM held in memory (rom:/ is overwritten while a game loads).
static int16_t *wind_samples = NULL;
static int wind_length = 0;
static waveform_t wind_waveform;

/**
 * @brief Reconfigure the sound system with the specified frequency.
 * 
 * @param frequency The audio frequency.
 */
static void sound_reconfigure (int frequency) {
    if ((frequency > 0) && (audio_get_frequency() != frequency)) {
        
        sound_deinit();

        audio_init(frequency, NUM_BUFFERS);
        mixer_init(NUM_CHANNELS);

        // Attempt to initialize wav64 compression level 1
        wav64_init_compression(1);
        sound_initialized = true;

        if (sfx_enabled) {
            sound_init_sfx();
        }
    }
}

/**
 * @brief Initialize the default sound system.
 */
void sound_init_default (void) {
    sound_reconfigure(DEFAULT_FREQUENCY);
}

/**
 * @brief Initialize the sound effects.
 */
void sound_init_sfx (void) {
    mixer_ch_set_vol(SOUND_SFX_CHANNEL, 0.5f, 0.5f);
    wav64_open(&sfx_cursor, "rom:/cursorsound.wav64");
    wav64_open(&sfx_exit, "rom:/back.wav64");
    wav64_open(&sfx_setting, "rom:/settings.wav64");
    wav64_open(&sfx_enter, "rom:/enter.wav64");
    wav64_open(&sfx_error, "rom:/error.wav64");
    sfx_enabled = true;
    sfx_opened = true;
}

/**
 * @brief Enable or disable sound effects.
 * 
 * @param state True to enable, false to disable.
 */
void sound_use_sfx(bool state) {
    sfx_enabled = state;
}

/**
 * @brief Play a sound effect.
 * 
 * @param sfx The sound effect to play.
 */
void sound_play_effect(sound_effect_t sfx) {
    if(sfx_enabled) {
        switch (sfx) {
            case SFX_CURSOR:
                wav64_play(&sfx_cursor, SOUND_SFX_CHANNEL);
                break;
            case SFX_EXIT:
                wav64_play(&sfx_exit, SOUND_SFX_CHANNEL);
                break;
            case SFX_SETTING:
                wav64_play(&sfx_setting, SOUND_SFX_CHANNEL);
                break;
            case SFX_ENTER:
                wav64_play(&sfx_enter, SOUND_SFX_CHANNEL);
                break;
            case SFX_ERROR:
                wav64_play(&sfx_error, SOUND_SFX_CHANNEL);
                break;
            default:
                break;
        } 
    }
}

/**
 * @brief Deinitialize the sound system.
 */
void sound_play_boot (void) {
    if (!sound_initialized || boot_sound_opened) {
        return;
    }
    wav64_open(&boot_sound, "rom:/boot/eclipse_boot_hall.wav64");
    boot_sound_opened = true;
    mixer_ch_set_vol(SOUND_BOOT_CHANNEL, 1.0f, 1.0f);
    wav64_play(&boot_sound, SOUND_BOOT_CHANNEL);
}

void sound_stop_boot (void) {
    if (boot_sound_opened) {
        mixer_ch_stop(SOUND_BOOT_CHANNEL);
        wav64_close(&boot_sound);
        boot_sound_opened = false;
    }
}

static void wind_read (void *ctx, samplebuffer_t *sbuf, int wpos, int wlen, bool seeking) {
    int16_t *dst = (int16_t *) samplebuffer_append(sbuf, wlen);
    for (int i = 0; i < wlen; i++) {
        dst[i] = (wpos + i < wind_length) ? wind_samples[wpos + i] : 0;
    }
}

void sound_loading_wind_prepare (void) {
    if (!sound_initialized || !sfx_enabled || wind_samples) {
        return;
    }
    int size;
    uint8_t *wav = asset_load("rom:/loading_wind.wav", &size);
    // A plain 44-byte header (make_sounds.py): 16-bit mono PCM, little-endian samples.
    if (!wav || size <= 44) {
        free(wav);
        return;
    }
    uint32_t rate = wav[24] | (wav[25] << 8) | (wav[26] << 16) | (wav[27] << 24);
    wind_length = (size - 44) / 2;
    wind_samples = malloc(wind_length * sizeof(int16_t));
    for (int i = 0; i < wind_length; i++) {
        wind_samples[i] = (int16_t) (wav[44 + (i * 2)] | (wav[45 + (i * 2)] << 8));
    }
    free(wav);

    wind_waveform = (waveform_t) {
        .name = "loading_wind",
        .bits = 16,
        .channels = 1,
        .frequency = (float) rate,
        .len = wind_length,
        .read = wind_read,
    };
}

void sound_loading_wind_play (void) {
    if (wind_samples) {
        mixer_ch_set_vol(SOUND_LOADING_CHANNEL, 0.5f, 0.5f);
        mixer_ch_play(SOUND_LOADING_CHANNEL, &wind_waveform);
    }
}

void sound_loading_wind_free (void) {
    if (wind_samples) {
        mixer_ch_stop(SOUND_LOADING_CHANNEL);
        free(wind_samples);
        wind_samples = NULL;
        wind_length = 0;
    }
}

void sound_deinit (void) {
    if (sound_initialized) {
        sound_loading_wind_free();
        if (boot_sound_opened) {
            mixer_ch_stop(SOUND_BOOT_CHANNEL);
            wav64_close(&boot_sound);
            boot_sound_opened = false;
        }
        if (sfx_opened) {
            wav64_close(&sfx_cursor);
            wav64_close(&sfx_exit);
            wav64_close(&sfx_setting);
            wav64_close(&sfx_enter);
            wav64_close(&sfx_error);
            sfx_opened = false;
        }
        mixer_close();
        audio_close();
        sound_initialized = false;
    }
}

/**
 * @brief Poll the sound system to process audio playback.
 */
void sound_poll (void) {
    if (sound_initialized) {
        
        // Check whether one audio buffer is ready, otherwise wait for next
        // frame to perform mixing.
        mixer_try_play();

        // The boot sound plays once; free it when it has finished.
        if (boot_sound_opened && !mixer_ch_playing(SOUND_BOOT_CHANNEL)) {
            wav64_close(&boot_sound);
            boot_sound_opened = false;
        }
    }
}
