ECLIPSE — TITLE INTRO

90 opaque RGB PNG frames, 640 x 480, 30 fps, 3 seconds, on the Eclipse
background colour #231A2E (the codeblaine.com page background).
Play eclipse_intro/intro_0001.png through intro_0090.png, then keep displaying
intro_0090.png until the title screen should end.

Timeline
  01–09: fade in sun and moon, initially separated by a 2 px edge gap.
  10–54: constant-speed approach along a shallow circular arc.
  55–57: corona fades in at totality.
  58–72: moon shifts 8 px right, clipping the corona into a C.
  73–81: remaining ECLIPSE letters fade in (Lexend Light, 90 px, plain I,
         12.5 px visible gaps, +4 px between I and P).
  82–90: hold the complete title.

Branding
  The title is the same artwork as the ECLIPSE logo on codeblaine.com and the
  cartridge label: the logo PNGs (eclipse_lexend_white_*.png) are exported from
  this title at 12x supersampling. The frames are rendered at the same 12x
  (SUPERSAMPLE = 12) and box-filtered down, so they match the logo exactly;
  rebuilding the 2048 px logo from this script is pixel-identical to it.

Regenerate
  Install Python 3 and current Pillow, then run:
    python3 make_eclipse_intro.py
  Keep fonts/Lexend.ttf beside the script in the fonts/ directory.
  The script needs no network access. Settings are named constants at its top.
  It recreates the PNGs, GIF, contact sheet, final title PNG and verification JSON.
  Then run scripts/make_boot_animation.py (from the repository root) to rebuild
  the menu's boot sprites.

Review
  preview.gif: all 90 frames; GIF's 10 ms timing uses 30/40 ms delays, averaging
               30 fps for a three-second loop.
  contact_sheet.png: frames 5, 10, ... 90, labelled.
  title_final.png: final held title.
  verification.json: measured checks for the generated frames.

Font source: https://github.com/googlefonts/lexend
Lexend is bundled unmodified; Light (300) is selected through its weight axis.
Copyright and SIL Open Font License are included in fonts/OFL.txt.
