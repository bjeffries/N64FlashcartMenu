/**
 * @file sound.c
 * @brief Sound component implementation
 * @ingroup ui_components
 */

#include <stdbool.h>
#include <string.h>
#include <libdragon.h>
#include "sound.h"
#include "utils/utils.h"

#define DEFAULT_FREQUENCY   (44100)
#define NUM_BUFFERS         (4)
#define NUM_CHANNELS        (16)

static wav64_t sfx_cursor, sfx_error, sfx_enter, sfx_exit, sfx_setting;

static bool sound_initialized = false;
static bool sfx_enabled = false;
static bool sfx_opened = false;

static wav64_t boot_sound;
static bool boot_sound_opened = false;

// "Game loaded" wind: 16-bit PCM held in memory (rom:/ is overwritten while a game loads).
static int16_t *wind_samples = NULL;    // interleaved if stereo
static int wind_length = 0;             // in frames (one sample per channel)
static int wind_channels = 1;
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
    for (int i = 0; i < wlen * wind_channels; i++) {
        int index = (wpos * wind_channels) + i;
        dst[i] = (index < wind_length * wind_channels) ? wind_samples[index] : 0;
    }
}

static uint32_t read_le (const uint8_t *p, int bytes) {
    uint32_t value = 0;
    for (int i = bytes - 1; i >= 0; i--) {
        value = (value << 8) | p[i];
    }
    return value;
}

void sound_loading_wind_prepare (void) {
    if (!sound_initialized || !sfx_enabled || wind_samples) {
        return;
    }
    int size;
    uint8_t *wav = asset_load("rom:/loading_wind.wav", &size);
    if (!wav || size < 12 || memcmp(wav, "RIFF", 4) != 0 || memcmp(wav + 8, "WAVE", 4) != 0) {
        free(wav);
        return;
    }
    // Walk the RIFF chunks for "fmt " (16-bit PCM, mono or stereo) and "data".
    uint32_t rate = 0;
    int channels = 0;
    const uint8_t *data = NULL;
    uint32_t data_size = 0;
    for (int offset = 12; offset + 8 <= size; ) {
        uint32_t chunk_size = read_le(wav + offset + 4, 4);
        const uint8_t *chunk = wav + offset + 8;
        if (memcmp(wav + offset, "fmt ", 4) == 0 && chunk_size >= 16) {
            bool pcm16 = (read_le(chunk, 2) == 1) && (read_le(chunk + 14, 2) == 16);
            channels = pcm16 ? (int) read_le(chunk + 2, 2) : 0;
            rate = read_le(chunk + 4, 4);
        } else if (memcmp(wav + offset, "data", 4) == 0) {
            data = chunk;
            data_size = MIN(chunk_size, (uint32_t) (size - (offset + 8)));
        }
        offset += 8 + chunk_size + (chunk_size & 1);
    }
    if (!data || (channels != 1 && channels != 2) || rate == 0) {
        debugf("[SOUND] loading_wind.wav: expected 16-bit PCM, mono or stereo\n");
        free(wav);
        return;
    }

    wind_channels = channels;
    wind_length = data_size / (2 * channels);
    wind_samples = malloc(wind_length * channels * sizeof(int16_t));
    for (int i = 0; i < wind_length * channels; i++) {
        wind_samples[i] = (int16_t) read_le(data + (i * 2), 2);
    }
    free(wav);

    wind_waveform = (waveform_t) {
        .name = "loading_wind",
        .bits = 16,
        .channels = channels,
        .frequency = (float) rate,
        .len = wind_length,
        .read = wind_read,
    };
}

void sound_loading_wind_play (void) {
    if (wind_samples) {
        mixer_ch_set_vol(SOUND_LOADING_CHANNEL, 1.0f, 1.0f);    // as provided, like the boot cue
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
