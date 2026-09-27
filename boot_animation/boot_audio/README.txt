ECLIPSE CART — PROCEDURAL BOOT AUDIO

eclipse_boot_hall.mp3 / .wav
  Recommended version: 6.5 seconds including the hall tail.
  Start with animation frame 1. The second chord starts at 1.8 seconds,
  exactly at frame 55. Hold the title after frame 90 while the reverb fades.

eclipse_boot_3s.mp3 / .wav
  Exact 3-second alternative. Identical cue with a 400 ms end fade.

animation_with_sound.mp4
  Review video pairing the full cue with the Oxanium Light revision.

Music: Gsus4 to Cadd9, soft additive synthesizer with subtle detuning and
octave chime. Large synthetic stereo hall, nominal 3.8-second RT60 with
45 ms pre-delay and shorter high-frequency decay. No external samples.
44.1 kHz stereo; MP3 at 192 kbps; lossless 16-bit WAV also provided.

REGENERATE
  python3 -m pip install numpy
  Install FFmpeg, then run:
    python3 make_boot_audio.py
  Or specify its location:
    python3 make_boot_audio.py --ffmpeg /path/to/ffmpeg
  The script recreates both WAVs, both MP3s and verification.json.
  The review video is a separate export, not recreated by this script.

EDIT THE NAMED CONSTANTS AT THE TOP OF THE SCRIPT
  FIRST_CHORD / SECOND_CHORD: note names and voicing.
  RESOLUTION_TIME: derives from TOTALITY_FRAME and ANIMATION_FPS.
  HARMONICS / CHIME_GAIN: softness and brightness of the synth.
  ATTACK_SECONDS / hold and release constants: note envelope.
  HALL_RT60_SECONDS: length of hall decay.
  HALL_WET_GAIN: amount of reverb.
  HALL_PREDELAY_SECONDS: separation between notes and reflections.
  PEAK_DBFS: output level.
  FULL_LENGTH_SECONDS: available space for the reverb tail.

verification.json reports duration, sample rate, channels, peak level and
clipping checks after decoding the MP3 files.
