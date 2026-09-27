# SummerCart64 hardware checklist

Everything so far was tested in the Ares emulator, which can't emulate the SummerCart64 or its
SD card. This list covers what only real hardware can show. Work top to bottom: later sections
depend on earlier ones, so a failure early on usually explains failures further down.

When something is wrong, note the section and item number, and if you can, take a photo of the
screen and capture the debug log (see [Getting a debug log](#getting-a-debug-log)).

---

## 1. Before you start

- [ ] **1.1 SC64 firmware is 2.20.2 or newer.** Check or update it with `sc64deployer` from the
      [SummerCart64 releases](https://github.com/Polprzewodnikowy/SummerCart64/releases)
      (`sc64deployer info`, then `sc64deployer firmware update <file>`).
- [ ] **1.2 SD card is FAT32 or exFAT.**
- [ ] **1.3 Games are in folders on the card** (for example `/N64/`), as `.z64`, `.n64` or `.v64`.

## 2. Prepare the SD card

With the card mounted on your Mac:

```sh
cd n64_flashcarts
./dev.sh sd /Volumes/YOUR_SD_CARD
```

This builds the menu without the emulator test data and copies:

| On the SD card | What it is |
|---|---|
| `/sc64menu.n64` | the menu |
| `/menu/labels.db` | cartridge label art |
| `/menu/metadata/…/metadata.ini` | titles, developer, publisher, year, players, descriptions |

It lists any games it couldn't find metadata for. Run it again whenever you add games or change
the metadata JSON.

- [ ] **2.1 The command finishes and lists your games.**

## 3. First boot

- [ ] **3.0 The Eclipse Cart animation plays** at power-on (about 4 seconds, ending in a fade),
      smoothly and without flicker on "ECLIPSE" or "CART".
- [ ] **3.0a Its sound plays** with it (the second chord lands as the corona appears), and the
      reverb tail carries on into the Library for about 2.5 s **without crackles or stutters**,
      even when the Library opens a folder with many games. *If it stutters, note the folder size.*
- [ ] **3.1 The menu starts on the Library tab** within a few seconds, with no error screen.
- [ ] **3.2 Your folders appear as folder tiles**; `menu` and system folders are hidden.
- [ ] **3.3 Opening a folder shows cartridges with labels** for games in `labels.db`, grey
      placeholders for the rest.
- [ ] **3.4 Titles come from the metadata** (e.g. "The Legend of Zelda: Ocarina of Time").
- [ ] **3.5 Settings > Flashcart Information** shows SummerCart64, your firmware version, and
      Yes for Real-Time Clock and Save Writeback. **Button** changes to Pressed while you hold
      the cartridge's button. **Voltage / Temp** shows readings.

## 4. Picture on the TV

- [ ] **4.1 Text is readable on the CRT**, especially the small font (captions, info rows,
      About page). Note anything that is hard to read.
- [ ] **4.2 No shimmering or flicker** on thin lines: the line above the button hints on
      settings screens, the 2px selection outline, the cheat digit underline. (Interlaced 480i
      makes 1px horizontal lines flicker.)
- [ ] **4.3 Nothing important is cut off at the screen edges** (overscan). The L/R icons, the
      right end of the button hints, and the page dots are the most exposed.
- [ ] **4.4 Colours look right**: gold favourite outline, button icon colours, grey badges.

## 5. Scrolling and speed

These depend on SD card speed, which the emulator doesn't reproduce.

- [ ] **5.1 Tap left/right:** each move slides smoothly with no pause.
- [ ] **5.2 Hold left/right in a folder with 30+ games:** scrolling keeps up without stutters.
      *If it stutters, note roughly how many games are in the folder.* (Each new cartridge reads
      its label and metadata from the SD card.)
- [ ] **5.3 Info panel updates promptly** when you stop on a game.
- [ ] **5.4 Large folder (200+ games):** opens without an error. On a console **without an
      Expansion Pak** the limit is 1024 entries per folder; with one there is no fixed limit.
- [ ] **5.5 L/R between tabs** feels instant.

## 6. Playing games

- [ ] **6.1 A plays a game**: loading screen with progress, then the game boots.
- [ ] **6.2 Saves work**: save in-game, power off, power on, load the save. Try one each of
      EEPROM, SRAM and FlashRAM games if you have them (e.g. Super Mario 64, Zelda: Ocarina of
      Time, Paper Mario).
- [ ] **6.3 Reset during a game:** with **Fast Reboot** off (Menu Settings) you return to the
      menu; with it on, the game restarts.
      Returning to the menu this way **doesn't** play the boot animation.
- [ ] **6.4 History:** after playing, the game appears first in the History tab.
- [ ] **6.5 Last Played** on the game's info page shows today's date (needs the clock, section 9).
- [ ] **6.6 Expansion Pak games** (e.g. Donkey Kong 64, Majora's Mask) boot; without the pak
      you get the warning instead.

## 7. Settings that are saved to the SD card

Power the console off and on after each change to confirm it was saved.

- [ ] **7.1 Menu Settings** toggles keep their values. With **Boot Animation** off, power-on goes straight to
      the Library.
- [ ] **7.2 Config (C-Right)**: change a game's Save Type, then reopen Config: the value is kept,
      and a `.ini` file appears next to the ROM.
- [ ] **7.3 Favorites (C-Left)** keep their order (alphabetical) after a restart.
- [ ] **7.4 Hide (C-Up)**: the game stays hidden after a restart; `menu/hidden.txt` lists it.
      **Show Hidden Games** brings it back greyed out, and C-Up unhides it.
- [ ] **7.5 Start Folder**: in the Library select a folder and press C-Left (Set to Default),
      restart: the Library opens in that folder. Menu Settings > Start Folder shows it, and A there
      resets it to the top level.

## 8. Cheats (needs an Expansion Pak)

- [ ] **8.1 Config > Cheat Codes**: add a known code for a game, name it, go back. A `.datel`
      file appears next to the ROM.
- [ ] **8.2 Turn Cheats on** in Config and play: the cheat works.
- [ ] **8.3 Without an Expansion Pak**, Cheat Codes shows "Needs Expansion Pak".

## 9. Clock

- [ ] **9.1 Settings > Time** shows the right date and time.
- [ ] **9.2 Adjust it** (A, change a field, A): the new time shows immediately and survives a
      power cycle.

## 10. Controller Pak

Use a pak you don't mind testing with, or back it up first.

- [ ] **10.1 Settings > Controller Pak Manager** shows Status "Controller Pak", free space, and
      one row per note.
- [ ] **10.2 Back Up Whole Pak**: "Backup saved to cpak_saves/…"; the file is on the SD card.
- [ ] **10.3 Back up a single note** (A on a note > Back Up Note).
- [ ] **10.4 Restore a Backup** lists both backups; restoring the note puts it back on the pak.
- [ ] **10.5 Removing the pak** while on this screen changes Status to "No Controller Pak"
      without errors.
- [ ] Optional: **10.6 Format Pak** and **Delete Note** (both ask first).

## 11. Sound

- [ ] **11.1 Menu sounds** are quiet and short on your TV/speakers, and **Sound Effects** off
      in Menu Settings silences them.

## 12. If you have a PAL console

- [ ] **12.1** The menu displays correctly.
- [ ] **12.2 PAL60 Mode** switches to 60Hz, and back. (If the picture disappears, turn it off in
      `menu/config.ini` on the SD card: under `[menu]`, set `pal60 = false`.)

---

## Getting a debug log

The menu prints what it is doing (and any error details) over USB.

1. Connect the SC64 to your Mac with USB and power on the console.
2. Run `sc64deployer debug` in a terminal.
3. Reproduce the problem, then copy the terminal output.

A crash shows a blue "CPU Exception" screen; a photo of it is very useful.

## Updating the menu later

`./dev.sh sd /Volumes/YOUR_SD_CARD` replaces `sc64menu.n64` and refreshes the labels and
metadata; your settings, favorites, history, hidden games, saves and cheats are kept.
