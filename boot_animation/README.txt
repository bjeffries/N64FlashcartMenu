ECLIPSE CART — TITLE INTRO

90 opaque RGB PNG frames, 640 x 480, 30 fps, 3 seconds.
Play eclipse_intro/intro_0001.png through intro_0090.png, then keep displaying
intro_0090.png until the title screen should end.

Timeline
  01–09: fade in sun and moon, initially separated by a 2 px edge gap.
  10–54: constant-speed approach along a shallow circular arc.
  55–57: corona fades in at totality.
  58–72: black moon shifts 8 px right, clipping the corona into a C.
  73–81: remaining ECLIPSE letters fade in, Oxanium Medium at 93 px.
  82–90: hold the complete title.

At totality, the corona becomes the title outline and the solar disc stays
hidden during the C transition. The final frame deliberately contains the
title; the earlier black-ending/no-text requirements have been superseded.

Regenerate
  Install Python 3 and current Pillow, then run:
    python3 make_eclipse_intro.py
  Keep fonts/Oxanium.ttf beside the script in the fonts/ directory.
  The script needs no network access. Settings are named constants at its top.
  It recreates the PNGs, GIF, contact sheet, final title PNG and verification JSON.

Review
  preview.gif: all 90 frames; GIF's 10 ms timing uses 30/40 ms delays, averaging
               30 fps for a three-second loop.
  contact_sheet.png: frames 5, 10, ... 90, labelled.
  title_final.png: final held title.
  verification.json: measured checks for the generated frames.

Font source: https://github.com/google/fonts/tree/main/ofl/oxanium
Oxanium is bundled unmodified; Medium is selected through its weight axis.
Copyright and SIL Open Font License are included in fonts/OFL.txt.
