#!/usr/bin/env python3
"""Turn the spinning N64 logo GIF into smooth frames for the screenshot placeholder.

The Library shows it in the screenshot area whenever no screenshot is showing (the gallery is
off or the game has none). Reads assets/images/n64logo_anim.gif and writes

  assets/n64logo/NN.png          FRAMES frames, one turn at FPS (smooth at the menu's 30fps),
                                 scaled to HEIGHT px tall and padded to a 16-pixel multiple width,
                                 in the logo's own flat colours on a transparent background (CI4)
  src/menu/n64_logo_frames.h     frame count, size and timing

The GIF has 56 frames that are evenly timed but unevenly spaced (single and about double
rotation steps, and near-duplicates at the loop), which looks choppy when slowed down. So:
  1. its frames are retimed so the logo turns at a steady speed: each lasts in proportion to how
     much the logo changes before the next one (pixels that differ), and the order is reversed
     (REVERSE) so it turns the other way;
  2. ffmpeg's motion interpolation (minterpolate) makes FRAMES evenly spaced frames from them,
     over one loop (the first frame is repeated at the end, so the seam is interpolated too);
  3. every pixel is snapped back to the nearest of the logo's colours, or made transparent
     when nearest the key colour it was drawn on, so the frames stay flat and crisp.

Needs ffmpeg on the PATH. Usage: scripts/make_n64_logo.py
"""

import glob
import os
import shutil
import subprocess
import tempfile

from PIL import Image, ImageChops

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
GIF = os.path.join(ROOT, 'assets', 'images', 'n64logo_anim.gif')
OUT_IMAGES = os.path.join(ROOT, 'assets', 'n64logo')
OUT_HEADER = os.path.join(ROOT, 'src', 'menu', 'n64_logo_frames.h')

HEIGHT = 168            # GAME_INFO_SCREENSHOT_HEIGHT: the logo fills the screenshot area's height
SPIN_MS = 8000          # one full turn
FPS = 30                # the menu's frame rate: a new frame every refresh
FRAMES = SPIN_MS * FPS // 1000
REVERSE = True          # turn the other way
KEY = (255, 0, 255)     # background the frames are interpolated on (not a logo colour)


def gif_frames(path):
    """Every frame as RGBA, composited as the GIF's disposal says."""
    gif = Image.open(path)
    canvas = Image.new('RGBA', gif.size, (0, 0, 0, 0))
    for i in range(gif.n_frames):
        gif.seek(i)
        frame = gif.convert('RGBA')
        if gif.disposal_method == 2:
            canvas = Image.new('RGBA', gif.size, (0, 0, 0, 0))
        canvas.alpha_composite(frame)
        yield canvas.copy()


def changed_pixels(a, b):
    return ImageChops.difference(a, b).convert('L').point(lambda v: 255 if v else 0).histogram()[255]


def snap(image, colours):
    """Each pixel to the nearest logo colour (RGBA) or transparent if nearest the key."""
    targets = [KEY] + colours
    cache = {}
    out = []
    for p in image.convert('RGB').getdata():
        if p not in cache:
            best = min(targets, key=lambda c: sum((x - y) ** 2 for x, y in zip(p, c)))
            cache[p] = (0, 0, 0, 0) if best == KEY else best + (255,)
        out.append(cache[p])
    snapped = Image.new('RGBA', image.size)
    snapped.putdata(out)
    return snapped


def paletted(image):
    """RGBA with hard alpha to 'P', transparent first."""
    pixels_rgba = list(image.getdata())
    colours = sorted({p[:3] for p in pixels_rgba if p[3] >= 128})
    index = {c: i + 1 for i, c in enumerate(colours)}
    pixels = bytes(index[p[:3]] if p[3] >= 128 else 0 for p in pixels_rgba)
    out = Image.frombytes('P', image.size, pixels)
    out.putpalette([v for c in [(0, 0, 0)] + colours for v in c])
    return out


def main():
    if not shutil.which('ffmpeg'):
        raise SystemExit('ffmpeg is needed (brew install ffmpeg)')

    source = list(gif_frames(GIF))
    if REVERSE:
        source.reverse()
    colours = sorted({p[:3] for f in source for p in f.getdata() if p[3] >= 128})
    count = len(source)
    steps = [max(1, changed_pixels(source[i], source[(i + 1) % count])) for i in range(count)]
    total = sum(steps)

    with tempfile.TemporaryDirectory() as work:
        # The source frames on the key colour, each lasting its share of the turn (a concat list
        # gives ffmpeg those durations); the first frame again at the end closes the loop.
        lines = []
        for i, frame in enumerate(source + [source[0]]):
            path = os.path.join(work, f'in_{i:03d}.png')
            flat = Image.new('RGBA', frame.size, KEY + (255,))
            flat.alpha_composite(frame)
            flat.convert('RGB').save(path)
            lines.append(f"file '{path}'")
            if i < count:
                lines.append(f'duration {steps[i] * SPIN_MS / total / 1000:.6f}')
        lines.append(f"file '{os.path.join(work, f'in_{count:03d}.png')}'")
        concat = os.path.join(work, 'frames.txt')
        with open(concat, 'w') as f:
            f.write('\n'.join(lines) + '\n')

        subprocess.run([
            'ffmpeg', '-hide_banner', '-loglevel', 'error', '-y',
            '-f', 'concat', '-safe', '0', '-i', concat,
            '-vf', f'minterpolate=fps={FPS}:mi_mode=mci:mc_mode=aobmc:me_mode=bidir:vsbmc=1',
            '-frames:v', str(FRAMES), os.path.join(work, 'out_%04d.png'),
        ], check=True)
        interpolated = sorted(glob.glob(os.path.join(work, 'out_*.png')))

        os.makedirs(OUT_IMAGES, exist_ok=True)
        for old in glob.glob(os.path.join(OUT_IMAGES, '*.png')):
            os.remove(old)
        padded_width = 0
        for n, path in enumerate(interpolated):
            frame = snap(Image.open(path), colours)
            width = round(frame.width * HEIGHT / frame.height)
            scaled = frame.resize((width, HEIGHT), Image.Resampling.NEAREST)
            padded_width = (width + 15) // 16 * 16
            canvas = Image.new('RGBA', (padded_width, HEIGHT), (0, 0, 0, 0))
            canvas.paste(scaled, ((padded_width - width) // 2, 0))
            paletted(canvas).save(os.path.join(OUT_IMAGES, f'{n:03d}.png'), transparency=0)
        frames = len(interpolated)

    header = [
        '// Generated by scripts/make_n64_logo.py from assets/images/n64logo_anim.gif - do not edit.',
        '',
        '#ifndef N64_LOGO_FRAMES_H__',
        '#define N64_LOGO_FRAMES_H__',
        '',
        f'#define N64_LOGO_FRAMES     ({frames})',
        f'#define N64_LOGO_WIDTH      ({padded_width})    // padded; the logo is centred in it',
        f'#define N64_LOGO_HEIGHT     ({HEIGHT})',
        f'#define N64_LOGO_LOOP_MS    ({SPIN_MS})   // one turn, frames evenly spaced in it',
        '',
        '#endif',
        '',
    ]
    with open(OUT_HEADER, 'w', newline='\n') as f:
        f.write('\n'.join(header))
    print(f'{frames} frames ({count} interpolated), {padded_width}x{HEIGHT}, one turn in {SPIN_MS / 1000:g}s at {FPS}fps')


if __name__ == '__main__':
    main()
