#!/usr/bin/env python3
"""Generate the menu's sound effects: short, soft sine tones.

Writes 44.1 kHz mono 16-bit WAVs into assets/sounds/ (converted to wav64 by the build):
  cursorsound.wav  moving the selection - a very short, quiet tick
  enter.wav        select / open        - two soft rising notes
  back.wav         back / cancel        - the same two notes, falling
  settings.wav     toggle / change      - a single gentle tone
  error.wav        something failed     - two low, soft falling notes

Every note has a few milliseconds of fade-in (no clicks) and a smooth exponential decay,
with a touch of the second harmonic for warmth. Tweak NOTES / VOLUME below and rebuild.

Usage: scripts/make_sounds.py [out_dir=assets/sounds]
"""

import math
import os
import struct
import sys
import wave

RATE = 44100
VOLUME = 0.30           # overall level; the menu also plays effects at half volume

# (frequency Hz, start ms, length ms, decay ms, level)
NOTES = {
    'cursorsound': [(523, 0, 40, 12, 0.6)],                         # C5: root of the boot cue's final chord
    'enter': [(784, 0, 70, 22, 0.8), (1175, 45, 110, 34, 0.8)],     # G5 -> D6
    'back': [(1175, 0, 70, 22, 0.7), (784, 45, 110, 34, 0.7)],      # D6 -> G5
    'settings': [(659, 0, 90, 26, 0.7)],                            # E5: third of the boot cue's final chord
    'error': [(294, 0, 90, 30, 1.0), (196, 70, 150, 45, 1.0)],      # D4 -> G3
}

ATTACK_MS = 4
HARMONIC = 0.12         # level of the 2nd harmonic relative to the fundamental


def render(notes):
    length = max(start + dur for _, start, dur, _, _ in notes)
    samples = [0.0] * int(RATE * length / 1000)
    for freq, start, dur, decay, level in notes:
        first = int(RATE * start / 1000)
        count = int(RATE * dur / 1000)
        for n in range(count):
            t = n / RATE
            attack = min(1.0, (t * 1000) / ATTACK_MS)
            envelope = attack * math.exp(-(t * 1000) / decay)
            # Fade the last 5 ms to exactly zero so the note ends without a click.
            tail = min(1.0, (count - n) / (RATE * 0.005))
            wave_value = math.sin(2 * math.pi * freq * t) + HARMONIC * math.sin(4 * math.pi * freq * t)
            samples[first + n] += level * envelope * tail * wave_value
    peak = max(1e-9, max(abs(s) for s in samples))
    scale = VOLUME / max(1.0, peak)
    return [int(max(-1.0, min(1.0, s * scale)) * 32767) for s in samples]


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else 'assets/sounds'
    for name, notes in NOTES.items():
        data = render(notes)
        path = os.path.join(out_dir, name + '.wav')
        with wave.open(path, 'wb') as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(RATE)
            w.writeframes(struct.pack('<%dh' % len(data), *data))
        print(f'{path}: {len(data) / RATE * 1000:.0f} ms')


if __name__ == '__main__':
    main()
