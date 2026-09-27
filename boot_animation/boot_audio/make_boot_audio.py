#!/usr/bin/env python3
"""Procedural Eclipse boot cue. Requires Python 3, NumPy and FFmpeg.

Run: python3 make_boot_audio.py --ffmpeg /path/to/ffmpeg
No recordings, samples, soundfonts or downloaded impulse responses are used.
"""
import argparse
import json
import math
import shutil
import subprocess
import wave
from pathlib import Path
import numpy as np

# Musical and visual timing. Frame 55 begins at (55-1)/30 = 1.8 seconds.
SAMPLE_RATE = 44100
ANIMATION_FPS = 30
TOTALITY_FRAME = 55
RESOLUTION_TIME = (TOTALITY_FRAME - 1) / ANIMATION_FPS
FIRST_CHORD_TIME = 0.08
FIRST_CHORD = ('G2', 'D3', 'G3', 'C4', 'D4')  # G suspended fourth
SECOND_CHORD = ('C3', 'G3', 'C4', 'D4', 'E4', 'G4')  # C add9
FIRST_GAIN, SECOND_GAIN = 0.72, 1.0
FIRST_HOLD, FIRST_RELEASE = 1.33, 0.37
SECOND_HOLD, SECOND_RELEASE = 0.32, 0.75
ATTACK_SECONDS = 0.035
HARMONICS = ((1, 1.0), (2, 0.20), (3, 0.085), (4, 0.025), (5, 0.012))
DETUNE_CENTS = 2.5
CHIME_GAIN = 0.045
STEREO_SPREAD = 0.42

# Large hall: long, dense reflections with shorter high-frequency decay.
HALL_RT60_SECONDS = 3.8
HALL_PREDELAY_SECONDS = 0.045
HALL_HIGH_FREQUENCY_RT60_SECONDS = 1.6
HALL_WET_GAIN = 0.50
EARLY_REFLECTIONS = ((0.063, 0.30), (0.091, 0.23), (0.137, 0.18), (0.191, 0.11))
REVERB_SEED = 64200
FULL_LENGTH_SECONDS = 6.5
SHORT_LENGTH_SECONDS = 3.0
SHORT_END_FADE_SECONDS = 0.40
FULL_END_FADE_SECONDS = 0.45
PEAK_DBFS = -2.0
MP3_BITRATE = '192k'
OUTPUT_ROOT = Path(__file__).resolve().parent


def frequency(note):
    semitones = {'C': 0, 'C#': 1, 'D': 2, 'D#': 3, 'E': 4, 'F': 5,
                 'F#': 6, 'G': 7, 'G#': 8, 'A': 9, 'A#': 10, 'B': 11}
    midi = (int(note[-1]) + 1) * 12 + semitones[note[:-1]]
    return 440 * 2 ** ((midi - 69) / 12)


def chord(notes, hold, release, gain):
    t = np.arange(round((hold + release) * SAMPLE_RATE)) / SAMPLE_RATE
    env = np.minimum(t / ATTACK_SECONDS, 1.0)
    env = np.sin(env * np.pi / 2) ** 2
    env *= 0.73 + 0.27 * np.exp(-t / 0.32)
    end = np.clip((t - hold) / release, 0, 1)
    env *= np.cos(end * np.pi / 2) ** 2
    result = np.zeros((len(t), 2))
    for index, note in enumerate(notes):
        f = frequency(note)
        voice = np.zeros(len(t))
        for harmonic, level in HARMONICS:
            phase = 0.11 * index * harmonic
            voice += level * np.sin(2 * np.pi * f * harmonic * t + phase) * np.exp(-t * (harmonic - 1) * 0.38)
        voice += 0.14 * np.sin(2 * np.pi * f * 2 ** (DETUNE_CENTS / 1200) * t)
        # Restrained bell-like octave sheen, strongest on the upper notes.
        if index >= len(notes) - 2:
            voice += CHIME_GAIN * np.sin(2 * np.pi * f * 4 * t) * np.exp(-t / 0.20)
        pan = (index / max(1, len(notes) - 1) - 0.5) * STEREO_SPREAD
        result[:, 0] += voice * env * math.cos((pan + 1) * np.pi / 4)
        result[:, 1] += voice * env * math.sin((pan + 1) * np.pi / 4)
    return result * gain / len(notes)


def hall_impulse(channel):
    """Deterministic synthetic diffuse hall IR with frequency-dependent decay."""
    count = round((HALL_RT60_SECONDS + 0.5) * SAMPLE_RATE)
    t = np.arange(count) / SAMPLE_RATE
    rng = np.random.default_rng(REVERB_SEED + channel)
    spectrum = np.fft.rfft(rng.normal(size=count))
    frequencies = np.fft.rfftfreq(count, 1 / SAMPLE_RATE)
    low_filter = 1 / (1 + (frequencies / 1800) ** 4)
    high_filter = (1 - low_filter) / (1 + (frequencies / 6500) ** 6)
    low = np.fft.irfft(spectrum * low_filter, n=count)
    high = np.fft.irfft(spectrum * high_filter, n=count)
    age = np.maximum(0, t - HALL_PREDELAY_SECONDS)
    build = np.minimum(age / 0.070, 1) ** 1.4
    ir = build * (low * np.exp(-math.log(1000) * age / HALL_RT60_SECONDS)
                  + 0.40 * high * np.exp(-math.log(1000) * age / HALL_HIGH_FREQUENCY_RT60_SECONDS))
    ir[t < HALL_PREDELAY_SECONDS] = 0
    ir *= 0.65 / np.sqrt(np.sum(ir ** 2))
    for delay, level in EARLY_REFLECTIONS:
        offset = round((delay + channel * 0.007) * SAMPLE_RATE)
        ir[offset] += level
    return ir


def convolve(signal, impulse):
    count = len(signal) + len(impulse) - 1
    fft_size = 1 << (count - 1).bit_length()
    return np.fft.irfft(np.fft.rfft(signal, fft_size) * np.fft.rfft(impulse, fft_size), fft_size)[:count]


def fade_end(audio, seconds):
    audio = audio.copy()
    count = min(len(audio), round(seconds * SAMPLE_RATE))
    audio[-count:] *= (np.cos(np.linspace(0, np.pi / 2, count)) ** 2)[:, None]
    audio[-1] = 0
    return audio


def write_audio(name, audio, ffmpeg):
    wav_path = OUTPUT_ROOT / (name + '.wav')
    mp3_path = OUTPUT_ROOT / (name + '.mp3')
    assert np.isfinite(audio).all() and np.max(np.abs(audio)) < 1
    pcm = np.round(audio * 32767).astype('<i2')
    with wave.open(str(wav_path), 'wb') as wav:
        wav.setnchannels(2)
        wav.setsampwidth(2)
        wav.setframerate(SAMPLE_RATE)
        wav.writeframes(pcm.tobytes())
    subprocess.run([ffmpeg, '-hide_banner', '-loglevel', 'error', '-y', '-i', str(wav_path),
                    '-codec:a', 'libmp3lame', '-b:a', MP3_BITRATE,
                    '-metadata', 'title=Eclipse Cart boot cue', str(mp3_path)], check=True)
    # Verify the encoded result, including decoded peak headroom and channel count.
    decoded = subprocess.run([ffmpeg, '-v', 'error', '-i', str(mp3_path), '-f', 'f32le',
                              '-acodec', 'pcm_f32le', '-'], check=True, capture_output=True)
    values = np.frombuffer(decoded.stdout, dtype='<f4').reshape(-1, 2)
    assert len(values) == len(audio), 'Unexpected decoded sample count'
    assert np.isfinite(values).all() and np.max(np.abs(values)) < 1
    return {'seconds': len(audio) / SAMPLE_RATE, 'sample_rate': SAMPLE_RATE, 'channels': 2,
            'wav_peak_dbfs': float(20 * np.log10(np.max(np.abs(audio)))),
            'mp3_decoded_peak_dbfs': float(20 * np.log10(np.max(np.abs(values)))),
            'mp3_decoded_samples_per_channel': len(values), 'clipping': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ffmpeg', default=shutil.which('ffmpeg'))
    args = parser.parse_args()
    if not args.ffmpeg:
        parser.error('Install FFmpeg or pass --ffmpeg /path/to/ffmpeg')
    dry = np.zeros((round(FULL_LENGTH_SECONDS * SAMPLE_RATE), 2))
    events = ((FIRST_CHORD_TIME, FIRST_CHORD, FIRST_HOLD, FIRST_RELEASE, FIRST_GAIN),
              (RESOLUTION_TIME, SECOND_CHORD, SECOND_HOLD, SECOND_RELEASE, SECOND_GAIN))
    for start, notes, hold, release, gain in events:
        sound = chord(notes, hold, release, gain)
        offset = round(start * SAMPLE_RATE)
        dry[offset:offset + len(sound)] += sound
    wet = np.zeros_like(dry)
    for channel in range(2):
        source = 0.65 * dry[:, channel] + 0.35 * dry[:, 1 - channel]
        wet[:, channel] = convolve(source, hall_impulse(channel))[:len(dry)]
    mixed = dry + HALL_WET_GAIN * wet
    mixed -= mixed.mean(axis=0)
    mixed *= 10 ** (PEAK_DBFS / 20) / np.max(np.abs(mixed))
    # Short fade-in removes the tiny DC-correction step during initial silence.
    mixed[:round(0.01 * SAMPLE_RATE)] *= np.linspace(0, 1, round(0.01 * SAMPLE_RATE))[:, None]
    full = fade_end(mixed, FULL_END_FADE_SECONDS)
    short = fade_end(mixed[:round(SHORT_LENGTH_SECONDS * SAMPLE_RATE)], SHORT_END_FADE_SECONDS)
    report = {'progression': ['Gsus4', 'Cadd9'], 'resolution_seconds': RESOLUTION_TIME,
              'resolution_frame': TOTALITY_FRAME, 'hall_rt60_seconds': HALL_RT60_SECONDS,
              'full': write_audio('eclipse_boot_hall', full, args.ffmpeg),
              'three_second': write_audio('eclipse_boot_3s', short, args.ffmpeg)}
    (OUTPUT_ROOT / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
