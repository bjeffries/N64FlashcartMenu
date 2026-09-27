#!/usr/bin/env python3
"""Render Eclipse Cart with Python 3 + Pillow. Run beside the output folder.

Geometry uses binary 4x masks sampled at subpixel centres, then BOX filtering.
No quantization or dithering is applied to the RGB PNG frames.
GIF has a 10 ms timebase: 30/30/40 ms delays average exactly 30 fps.
Keep the bundled fonts/ directory beside this script. No network is needed.
The final frame intentionally holds the title instead of fading to black.
"""
import json
import math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, GifImagePlugin

WIDTH, HEIGHT = 640, 480
FPS, FRAME_COUNT, SUPERSAMPLE = 30, 90, 4
SUN_CENTRE = (211.75, 200)
SUN_RADIUS = MOON_RADIUS = 28
MOON_START_GAP = 2
MOON_APPROACH_OFFSET = (-190, 40)
MOON_START_DISTANCE = SUN_RADIUS + MOON_RADIUS + MOON_START_GAP
MOON_ARC_BOW = 4  # Maximum perpendicular offset above the straight approach, px.
MOON_ARC_RADIUS = MOON_START_DISTANCE ** 2 / (8 * MOON_ARC_BOW) + MOON_ARC_BOW / 2
MOON_ARC_ANGLE = 2 * math.asin(MOON_START_DISTANCE / (2 * MOON_ARC_RADIUS))
MOON_START = tuple(c + offset * MOON_START_DISTANCE / math.hypot(*MOON_APPROACH_OFFSET)
                   for c, offset in zip(SUN_CENTRE, MOON_APPROACH_OFFSET))
CORONA_INNER_RADIUS, CORONA_OUTER_RADIUS = 29, 32
SUN_START_COLOUR, SUN_END_COLOUR = (184, 86, 26), (255, 255, 255)
MOON_START_COLOUR, MOON_END_COLOUR = (112, 112, 112), (0, 0, 0)
CORONA_COLOUR, BACKGROUND = (255, 255, 255), (0, 0, 0)
FADE_IN_START, FADE_IN_END = 1, 9
MOVE_START, MOVE_END = 10, 54
CORONA_START, CORONA_END = 55, 57
C_FORM_START, C_FORM_END = 58, 72
MOON_FINAL_RIGHT_SHIFT = 8
TEXT_FADE_START, TEXT_FADE_END = 73, 81
TITLE_HOLD_START, TITLE_HOLD_END = 82, 90
TITLE_TEXT, TITLE_FONT_SIZE, TITLE_FONT_WEIGHT = 'ECLIPSE', 93, 500
TITLE_SPACING, TITLE_C_SLOT_WIDTH = 6, 64
TITLE_OPTICAL_OFFSET_X = -2  # Centre visible ink, accounting for font sidebearings.
TITLE_COLOUR = (255, 255, 255)
SAFE_BOUNDS = (32, 24, 608, 456)
EMPTY_FROM_Y = 330
CONTACT_STEP, CONTACT_COLUMNS = 5, 3
THUMBNAIL_SIZE, LABEL_HEIGHT = (320, 240), 28
OUTPUT_ROOT = Path(__file__).resolve().parent
FRAME_FOLDER = OUTPUT_ROOT / 'eclipse_intro'
FONT_PATH = OUTPUT_ROOT / 'fonts' / 'Oxanium.ttf'


def title_mask():
    """Centre the selected title with a geometric C in its second slot."""
    font = ImageFont.truetype(str(FONT_PATH), TITLE_FONT_SIZE * SUPERSAMPLE)
    font.set_variation_by_axes([TITLE_FONT_WEIGHT])
    glyphs = []
    for letter in TITLE_TEXT:
        if letter == 'C':
            glyphs.append(Image.new('L', (TITLE_C_SLOT_WIDTH * SUPERSAMPLE, 64 * SUPERSAMPLE)))
        else:
            bounds = font.getbbox(letter)
            glyph = Image.new('L', (bounds[2] - bounds[0], bounds[3] - bounds[1]))
            ImageDraw.Draw(glyph).text((-bounds[0], -bounds[1]), letter, font=font, fill=255)
            # Binary subpixel membership keeps edge shades controlled; BOX supplies AA.
            glyphs.append(glyph.point(lambda value: 255 if value >= 128 else 0))
    width = sum(g.width for g in glyphs) + (len(glyphs) - 1) * TITLE_SPACING * SUPERSAMPLE
    left = round((WIDTH * SUPERSAMPLE - width) / 2 + TITLE_OPTICAL_OFFSET_X * SUPERSAMPLE)
    expected_c_x = (left + glyphs[0].width + TITLE_SPACING * SUPERSAMPLE + TITLE_C_SLOT_WIDTH * SUPERSAMPLE / 2) / SUPERSAMPLE
    assert expected_c_x == SUN_CENTRE[0], 'Update SUN_CENTRE to match the title layout'
    mask = Image.new('L', (WIDTH * SUPERSAMPLE, HEIGHT * SUPERSAMPLE))
    x = left
    for glyph in glyphs:
        mask.paste(glyph, (x, round(SUN_CENTRE[1] * SUPERSAMPLE) - glyph.height // 2))
        x += glyph.width + TITLE_SPACING * SUPERSAMPLE
    return mask, {'font': 'Oxanium Medium', 'font_size_px': TITLE_FONT_SIZE,
                  'title_width_px': width / SUPERSAMPLE, 'title_left_px': left / SUPERSAMPLE,
                  'spacing_px': TITLE_SPACING, 'eclipse_centre': list(SUN_CENTRE)}


def srgb_to_linear(value):
    value /= 255
    return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4


def linear_to_srgb(value):
    value = 12.92 * value if value <= 0.0031308 else 1.055 * value ** (1 / 2.4) - 0.055
    return max(0, min(255, int(value * 255 + 0.5)))


def colour_lerp(start, end, progress):
    return tuple(linear_to_srgb(srgb_to_linear(a) * (1 - progress) + srgb_to_linear(b) * progress)
                 for a, b in zip(start, end))


def scaled(colour, opacity):
    return tuple(int(c * opacity + 0.5) for c in colour)


def disc_mask(centre, radius):
    """Exact circle membership at 4x sample centres (no inclusive ellipse bias)."""
    scale = SUPERSAMPLE
    mask = Image.new('L', (WIDTH * scale, HEIGHT * scale), 0)
    draw = ImageDraw.Draw(mask)
    cx, cy = (c * scale for c in centre)
    r = radius * scale
    for y in range(max(0, math.ceil(cy - r - 0.5)), min(HEIGHT * scale, math.floor(cy + r - 0.5) + 1)):
        span = math.sqrt(max(0, r * r - (y + 0.5 - cy) ** 2))
        left, right = math.ceil(cx - span - 0.5), math.floor(cx + span - 0.5)
        if right >= left:
            draw.line((left, y, right, y), fill=255)
    return mask


def moon_position(t):
    """Uniform angular motion on a circular arc gives exactly constant speed."""
    if t <= 0:
        return MOON_START
    if t >= 1:
        return SUN_CENTRE
    ux, uy = ((b - a) / MOON_START_DISTANCE for a, b in zip(MOON_START, SUN_CENTRE))
    normal = (uy, -ux)
    midpoint = tuple((a + b) / 2 for a, b in zip(MOON_START, SUN_CENTRE))
    angle = (t - 0.5) * MOON_ARC_ANGLE
    along = MOON_ARC_RADIUS * math.sin(angle)
    bow = MOON_ARC_RADIUS * (math.cos(angle) - math.cos(MOON_ARC_ANGLE / 2))
    return tuple(m + u * along + n * bow for m, u, n in zip(midpoint, (ux, uy), normal))


def state(frame):
    if frame < MOVE_START:
        t = 0.0
    else:
        t = min(1.0, (frame - MOVE_START + 1) / (MOVE_END - MOVE_START + 1))
    centre = moon_position(t)
    distance = math.dist(centre, SUN_CENTRE)
    p = max(0.0, min(1.0, 1 - distance / (2 * SUN_RADIUS)))
    p = p * p * (3 - 2 * p)
    fade = min(1.0, (frame - FADE_IN_START) / (FADE_IN_END - FADE_IN_START))
    if frame >= C_FORM_START:
        shift = MOON_FINAL_RIGHT_SHIFT * min(1.0, (frame - C_FORM_START + 1) / (C_FORM_END - C_FORM_START + 1))
        centre = (SUN_CENTRE[0] + shift, SUN_CENTRE[1])
        p = 1.0  # Colours stay locked at totality during the title transition.
    corona = max(0.0, min(1.0, (frame - CORONA_START + 1) / (CORONA_END - CORONA_START + 1)))
    return centre, p, fade, corona


def write_gif(frames, path):
    """Write every frame explicitly, including identical hold frames."""
    with path.open('wb') as file:
        for index, frame in enumerate(frames):
            palette = frame.quantize(colors=256, dither=Image.Dither.NONE)
            if index == 0:
                header, _ = GifImagePlugin.getheader(palette, info={'loop': 0})
                for block in header:
                    file.write(block)
            delay = (round((index + 1) * 100 / FPS) - round(index * 100 / FPS)) * 10
            for block in GifImagePlugin.getdata(palette, duration=delay, disposal=1, include_color_table=True):
                file.write(block)
        file.write(b';')


def verify(frames, ring_mask, text_mask, layout):
    gap = math.dist(MOON_START, SUN_CENTRE) - SUN_RADIUS - MOON_RADIUS
    assert math.isclose(gap, MOON_START_GAP, abs_tol=1e-10)
    positions = [state(f)[0] for f in range(MOVE_START - 1, MOVE_END + 1)]
    steps = [math.dist(a, b) for a, b in zip(positions, positions[1:])]
    assert max(steps) - min(steps) < 1e-10
    midpoint = tuple((a + b) / 2 for a, b in zip(MOON_START, SUN_CENTRE))
    assert math.isclose(math.dist(moon_position(0.5), midpoint), MOON_ARC_BOW, abs_tol=1e-10)
    speed = MOON_ARC_RADIUS * MOON_ARC_ANGLE / ((MOVE_END - MOVE_START + 1) / FPS)
    paths = sorted(FRAME_FOLDER.glob('intro_*.png'))
    assert [p.name for p in paths] == [f'intro_{i:04d}.png' for i in range(1, FRAME_COUNT + 1)]
    counts, bounds = [], []
    for path in paths:
        with Image.open(path) as im:
            assert im.size == (WIDTH, HEIGHT) and im.mode == 'RGB'
            colours = im.getcolors(WIDTH * HEIGHT)
            counts.append(len(colours))
            assert len(colours) <= 256, (path, len(colours))
            bbox = im.getbbox()
            if bbox:
                x0, y0, x1, y1 = bbox
                assert x0 >= SAFE_BOUNDS[0] and y0 >= SAFE_BOUNDS[1]
                assert x1 - 1 <= SAFE_BOUNDS[2] and y1 - 1 <= SAFE_BOUNDS[3]
                assert y1 <= EMPTY_FROM_Y
                bounds.append(bbox)
    assert frames[0].getbbox() is None and frames[-1].getbbox() is not None
    assert all(frame.tobytes() == frames[-1].tobytes() for frame in frames[TEXT_FADE_END - 1:])
    c_frame = frames[C_FORM_END - 1]
    # Verify the text-free C and the full title independently against their masks.
    expected_c = Image.new('RGB', ring_mask.size, BACKGROUND)
    expected_c.paste(CORONA_COLOUR, (0, 0), ring_mask)
    final_moon_centre = (SUN_CENTRE[0] + MOON_FINAL_RIGHT_SHIFT, SUN_CENTRE[1])
    expected_c.paste(BACKGROUND, (0, 0), disc_mask(final_moon_centre, MOON_RADIUS))
    assert c_frame.tobytes() == expected_c.resize((WIDTH, HEIGHT), Image.Resampling.BOX).tobytes()
    expected_c.paste(TITLE_COLOUR, (0, 0), text_mask)
    assert frames[-1].tobytes() == expected_c.resize((WIDTH, HEIGHT), Image.Resampling.BOX).tobytes()
    assert frames[TEXT_FADE_START - 1].tobytes() != c_frame.tobytes()
    final_bounds = frames[-1].getbbox()
    assert abs((final_bounds[0] + final_bounds[2]) / 2 - WIDTH / 2) <= 1
    centre, p, _, _ = state(55)
    assert centre == SUN_CENTRE and p == 1
    # The entire totality frame must equal a ring alone: no residual sun at edges.
    ring_only = Image.new('RGB', ring_mask.size, BACKGROUND)
    ring_only.paste(scaled(CORONA_COLOUR, 1 / 3), (0, 0), ring_mask)
    ring_only = ring_only.resize((WIDTH, HEIGHT), Image.Resampling.BOX)
    assert frames[54].tobytes() == ring_only.tobytes()
    assert CORONA_OUTER_RADIUS - CORONA_INNER_RADIUS >= 2
    # Test the downsampled full-white ring on 3,600 radial rays. Every ray
    # must cross >=2 px of >=50% intensity, accounting for antialiasing.
    ring = frames[CORONA_END - 1].getchannel('R')
    min_width = float('inf')
    radial_step = 0.01
    for angle_index in range(3600):
        angle = angle_index * math.tau / 3600
        hit_count = 0
        for step in range(600):
            radius = SUN_RADIUS + (step + 0.5) * radial_step
            x = math.floor(SUN_CENTRE[0] + radius * math.cos(angle))
            y = math.floor(SUN_CENTRE[1] + radius * math.sin(angle))
            hit_count += ring.getpixel((x, y)) >= 128
        min_width = min(min_width, hit_count * radial_step)
    assert min_width >= 2
    with Image.open(OUTPUT_ROOT / 'preview.gif') as gif:
        assert gif.n_frames == FRAME_COUNT
        duration = 0
        for index in range(gif.n_frames):
            gif.seek(index)
            duration += gif.info['duration']
        assert duration == 3000
    report = {
        'moon_start_centre': list(MOON_START), 'moon_initial_edge_gap_px': gap,
        'moon_motion': 'constant speed on a shallow circular arc; no easing',
        'moon_speed_px_per_second': speed, 'moon_arc_bow_px': MOON_ARC_BOW,
        'moon_equal_frame_steps_verified': True,
        'frame_count': len(paths), 'size': [WIDTH, HEIGHT], 'mode': 'RGB', 'alpha': False,
        'max_distinct_colours': max(counts), 'first_frame_pure_black': True,
        'final_frame_holds_title': True, 'identical_full_title_frames': [TEXT_FADE_END, FRAME_COUNT],
        'text_fade_frames': [TEXT_FADE_START, TEXT_FADE_END],
        'c_formation_frames': [C_FORM_START, C_FORM_END],
        'final_moon_centre': list(final_moon_centre), 'title_layout': layout,
        'full_title_bounds_exclusive': list(final_bounds),
        'nonblack_bounds_inclusive': [min(b[0] for b in bounds), min(b[1] for b in bounds),
                                      max(b[2] for b in bounds) - 1, max(b[3] for b in bounds) - 1],
        'safe_area_pass': True, 'empty_from_y_330': True,
        'frame_55_moon_centre': list(centre), 'frame_55_only_corona_visible': True,
        'corona_geometric_thickness_px': CORONA_OUTER_RADIUS - CORONA_INNER_RADIUS,
        'corona_min_sampled_raster_thickness_px_at_half_intensity': round(min_width, 2),
        'gif_frames': FRAME_COUNT, 'gif_total_duration_ms': duration,
        'gif_timing_note': '30/40 ms frame delays; exactly 3 seconds, average 30 fps',
    }
    (OUTPUT_ROOT / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


def main():
    FRAME_FOLDER.mkdir(parents=True, exist_ok=True)
    text_mask, layout = title_mask()
    sun_mask = disc_mask(SUN_CENTRE, SUN_RADIUS)
    corona_mask = disc_mask(SUN_CENTRE, CORONA_OUTER_RADIUS)
    corona_mask.paste(0, (0, 0), disc_mask(SUN_CENTRE, CORONA_INNER_RADIUS))
    frames = []
    for number in range(1, FRAME_COUNT + 1):
        centre, p, fade, corona = state(number)
        image = Image.new('RGB', sun_mask.size, BACKGROUND)
        # At totality the corona takes over as the title outline. Keep the
        # solar disc hidden when the moon subsequently cuts the C opening.
        if number <= MOVE_END:
            image.paste(scaled(colour_lerp(SUN_START_COLOUR, SUN_END_COLOUR, p), fade), (0, 0), sun_mask)
        moon_mask = sun_mask if centre == SUN_CENTRE else disc_mask(centre, MOON_RADIUS)
        image.paste(scaled(colour_lerp(MOON_START_COLOUR, MOON_END_COLOUR, p), fade), (0, 0), moon_mask)
        if corona > 0:
            image.paste(scaled(CORONA_COLOUR, corona * fade), (0, 0), corona_mask)
        if number >= C_FORM_START:
            image.paste(BACKGROUND, (0, 0), moon_mask)
        if number >= TEXT_FADE_START:
            text_opacity = min(1.0, (number - TEXT_FADE_START + 1) / (TEXT_FADE_END - TEXT_FADE_START + 1))
            image.paste(scaled(TITLE_COLOUR, text_opacity), (0, 0), text_mask)
        image = image.resize((WIDTH, HEIGHT), Image.Resampling.BOX)
        image.save(FRAME_FOLDER / f'intro_{number:04d}.png')
        frames.append(image)
    write_gif(frames, OUTPUT_ROOT / 'preview.gif')
    selected = list(range(CONTACT_STEP, FRAME_COUNT + 1, CONTACT_STEP))
    tile_w, tile_h = THUMBNAIL_SIZE
    sheet = Image.new('RGB', (CONTACT_COLUMNS * tile_w, math.ceil(len(selected) / CONTACT_COLUMNS) * (tile_h + LABEL_HEIGHT)), '#202020')
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default(size=16)
    for index, number in enumerate(selected):
        x, y = (index % CONTACT_COLUMNS) * tile_w, (index // CONTACT_COLUMNS) * (tile_h + LABEL_HEIGHT)
        sheet.paste(frames[number - 1].resize(THUMBNAIL_SIZE, Image.Resampling.BOX), (x, y))
        draw.text((x + 10, y + tile_h + 5), f'Frame {number:04d}', fill='white', font=font)
    sheet.save(OUTPUT_ROOT / 'contact_sheet.png')
    frames[-1].save(OUTPUT_ROOT / 'title_final.png')
    verify(frames, corona_mask, text_mask, layout)


if __name__ == '__main__':
    main()
