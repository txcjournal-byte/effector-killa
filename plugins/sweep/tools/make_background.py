"""Generates the TrapWin SWEEP editor background (Resources/bg.png) at 2x of the 860x540 base size,
plus the photoreal knob images (Resources/knob_big.png, knob_small.png).
Run:  python3 tools/make_background.py

Layout constants here must match Source/PluginEditor.cpp.
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from brand import chrome_title, place, place_title, trapwin_logo  # noqa: E402
from knobgen import render_knob  # noqa: E402

S = 2
BW, BH = 860, 540
W, H = BW * S, BH * S
ROOT = os.path.dirname(HERE)
FONTS = os.path.join(ROOT, "Resources", "fonts")
rng = np.random.default_rng(31)
ACC = (32, 217, 138)
TEXT = (236, 230, 220)
INK = (146, 138, 126)

# ---- layout (base px) ---- must match Source/PluginEditor.cpp
FA = (16, 92, 248, 340)
FB = (612, 92, 844, 340)
BIG_A = (88, 204)
BIG_B = (772, 204)
BIG_R = 68                      # big knob box 136 px
COL_A = 204
COL_B = 656
COL_Y = [161, 231, 301]
SMALL_R = 25                    # small knob box 50 px
SCREEN = (260, 92, 600, 292)
LFO_DECK = (16, 350, 472, 528)
MOD_DECK = (484, 350, 844, 528)
PATTERN = (28, 386, 340, 470)
LFO_KX = [384, 442]
ROW_Y = [414, 486]
MOD_KX = [520, 590, 660, 730, 800]
LCD_A = (38, 284, 138, 306)
LCD_B = (722, 284, 822, 306)


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), int(size * S))


def to_img(a):
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))


class BlendDraw:
    """ImageDraw that alpha-blends (Pillow overwrites alpha on RGBA targets): each call draws on an overlay."""

    def __init__(self, holder, key):
        self.holder, self.key = holder, key

    def textlength(self, *a, **k):
        return ImageDraw.Draw(self.holder[self.key]).textlength(*a, **k)

    def __getattr__(self, name):
        def call(*a, **k):
            base = self.holder[self.key]
            layer = Image.new("RGBA", base.size, (0, 0, 0, 0))
            getattr(ImageDraw.Draw(layer), name)(*a, **k)
            self.holder[self.key] = Image.alpha_composite(base.convert("RGBA"), layer)
        return call


# ---------- chassis: graphite with fine horizontal brushing + warm key light ----------
base = np.zeros((H, W, 3), np.float32)
base[:] = (25, 25, 28)
yy, xx = np.mgrid[0:H, 0:W]
brush = rng.normal(0, 1, (H, 1)) * 2.2 + rng.normal(0, 1, (H, W)) * 1.2
brush = np.asarray(Image.fromarray(np.uint8(np.clip(128 + brush * 6, 0, 255))).filter(ImageFilter.BoxBlur(1)), np.float32) - 128
base += brush[..., None] * 0.42
light = 1.0 + 0.18 * (1.0 - (xx / W) * 0.4 - (yy / H))
base *= light[..., None]
glow = np.exp(-(((xx - W * 0.5) / (W * 0.45)) ** 2 + ((yy - H * 0.30) / (H * 0.42)) ** 2))
base += glow[..., None] * np.array([2, 12, 8])
vig = 1.0 - 0.38 * (((xx - W / 2) / (W / 2)) ** 2 + ((yy - H / 2) / (H / 2)) ** 2) ** 1.5
base *= vig[..., None]
img = to_img(base).convert("RGBA")
d = BlendDraw(globals(), "img")


def rr(box, r, **kw):
    x0, y0, x1, y1 = box
    d.rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=r * S, **kw)


def inset(box, r=12, fill=(14, 14, 16, 255), outline=(84, 78, 70, 150), shadow=200):
    """recessed panel: dark floor, blurred inner shadow, thin lit rim"""
    global img, d
    rr(box, r, fill=fill)
    sh = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    x0, y0, x1, y1 = box
    ImageDraw.Draw(sh).rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=r * S, outline=(0, 0, 0, shadow), width=5 * S)
    sh = sh.filter(ImageFilter.GaussianBlur(5 * S))
    m = Image.new("L", (W, H), 0)
    ImageDraw.Draw(m).rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=r * S, fill=255)
    sh.putalpha(Image.fromarray(np.minimum(np.asarray(sh.getchannel("A")), np.asarray(m))))
    img = Image.alpha_composite(img, sh)
    d = BlendDraw(globals(), "img")
    rr(box, r, outline=outline, width=S)
    # lower lip catches the light
    d.line(((x0 + r) * S, y1 * S + S, (x1 - r) * S, y1 * S + S), fill=(120, 112, 100, 70), width=S)


lab = font("BarlowCondensed-Bold.ttf", 15)
lab_big = font("BarlowCondensed-Bold.ttf", 18)
small = font("BarlowCondensed-Bold.ttf", 12)
tiny = font("BarlowCondensed-SemiBold.ttf", 11)


def label(text, cx, y, f=lab, col=TEXT + (255,), spacing=0.0):
    if spacing:
        widths = [d.textlength(ch, font=f) for ch in text]
        tw = sum(widths) + spacing * S * (len(text) - 1)
        x = cx * S - tw / 2
        for ch, w in zip(text, widths):
            d.text((x, y * S + S), ch, font=f, fill=(0, 0, 0, 170))
            d.text((x, y * S), ch, font=f, fill=col)
            x += w + spacing * S
        return
    tw = d.textlength(text, font=f)
    d.text((cx * S - tw / 2, y * S + S), text, font=f, fill=(0, 0, 0, 170))      # engraved shadow
    d.text((cx * S - tw / 2, y * S), text, font=f, fill=col)






# ---------- header ----------
img = place_title(img, chrome_title("SWEEP", 30 * S, glow=ACC, glow_strength=0.85), 24 * S, 14 * S)
_lg = trapwin_logo(36 * S)
img = place(img, _lg, 20 * S, 50 * S)
d = BlendDraw(globals(), "img")
d.text((20 * S + _lg.width + 12 * S, 62 * S), "FILTER MOTION  ·  LFO  ·  STEPS  ·  ENVELOPE", font=font("BarlowCondensed-SemiBold.ttf", 13), fill=INK + (255,))
hl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
hd = ImageDraw.Draw(hl)
for x in range(16 * S, (BW - 16) * S):
    t = (x / S - 16) / (BW - 32)
    hd.point((x, 85 * S), fill=ACC + (int(90 * math.sin(math.pi * t) ** 2),))
img = Image.alpha_composite(img, hl)
d = BlendDraw(globals(), "img")

lab_mid = font("BarlowCondensed-Bold.ttf", 13)


def big_seat(cx, cy):
    """recessed well for the big cutoff knob (arc radius 45 drawn by the editor)"""
    global img, d
    R = 52
    yy2, xx2 = np.mgrid[0:(2 * R + 8) * S, 0:(2 * R + 8) * S].astype(np.float32)
    cc = (R + 4) * S
    rr2 = np.sqrt((xx2 - cc) ** 2 + (yy2 - cc) ** 2) / S
    ang = np.arctan2(yy2 - cc, xx2 - cc)
    lip = (rr2 > R - 3) & (rr2 <= R + 2)
    floor = rr2 <= R - 3
    well = np.zeros(rr2.shape + (4,), np.float32)
    lit = 0.5 + 0.5 * np.cos(ang - math.radians(45))
    well[..., :3] = np.where(lip[..., None], (np.array([52, 54, 52]) + 100 * lit[..., None] ** 2), 0)
    shadowin = np.clip(1.0 - (R - 3 - rr2) / 14.0, 0, 1) * np.clip(0.5 - 0.5 * np.cos(ang - math.radians(45)), 0, 1)
    flo = np.array([12, 13, 13]) * (1.0 - 0.6 * shadowin[..., None])
    well[..., :3] = np.where(floor[..., None], flo, well[..., :3])
    well[..., 3] = np.where(lip | floor, 255, 0) * np.clip(R + 2 - rr2 + 0.5, 0, 1)
    img.alpha_composite(Image.fromarray(np.clip(well, 0, 255).astype(np.uint8), "RGBA"), (int((cx - R - 4) * S), int((cy - R - 4) * S)))
    d = BlendDraw(globals(), "img")
    d.arc(((cx - 45) * S, (cy - 45) * S, (cx + 45) * S, (cy + 45) * S), start=135, end=45, fill=(28, 36, 33, 255), width=4 * S)
    for i in range(21):
        a = math.radians(225 - 270 * i / 20)
        major = i % 5 == 0
        r0, r1 = 57, (64 if major else 60)
        d.line(((cx + math.cos(a) * r0) * S, (cy - math.sin(a) * r0) * S, (cx + math.cos(a) * r1) * S, (cy - math.sin(a) * r1) * S),
               fill=(222, 232, 226, 220 if major else 90), width=S + (1 if major else 0))
    for frac, t in ((0.0, "20"), (1.0, "20k")):
        a = math.radians(225 - 270 * frac)
        tx, ty = cx + math.cos(a) * 70, cy - math.sin(a) * 70
        tw = d.textlength(t, font=tiny)
        d.text((tx * S - tw / 2, (ty - 2) * S), t, font=tiny, fill=INK + (255,))


def small_seat(cx, cy, name):
    label(name, cx, cy - 37, lab_mid)
    for k in range(21):
        a = math.radians(225 - 270 * k / 20)
        x, y = cx + math.cos(a) * 22.5, cy - math.sin(a) * 22.5
        d.ellipse(((x - 1.3) * S, (y - 1.3) * S, (x + 1.3) * S, (y + 1.3) * S), fill=(40, 46, 43, 255))
    d.ellipse(((cx - 18.5) * S, (cy - 18.5) * S, (cx + 18.5) * S, (cy + 18.5) * S), fill=(7, 8, 8, 255))


def lcd(box):
    global img, d
    inset(box, 5, fill=(8, 9, 8, 255), outline=(60, 70, 66, 170), shadow=230)
    x0, y0, x1, y1 = box
    g = np.zeros(((y1 - y0 - 4) * S, (x1 - x0 - 4) * S, 4), np.float32)
    g[..., :3] = (5, 20, 14)
    g[::3, :, :3] *= 0.7
    g[..., 3] = 255
    gi = Image.fromarray(np.clip(g, 0, 255).astype(np.uint8), "RGBA")
    m = Image.new("L", gi.size, 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, gi.size[0] - 1, gi.size[1] - 1), radius=3 * S, fill=255)
    img.paste(gi, ((x0 + 2) * S, (y0 + 2) * S), m)
    d = BlendDraw(globals(), "img")


# ---------- filter panels ----------
for box, big, col, title, tx in ((FA, BIG_A, COL_A, "FILTER A", 28), (FB, BIG_B, COL_B, "FILTER B", 624)):
    inset(box, 12, fill=(16, 17, 17, 255))
    d.text((tx * S, 99 * S), title, font=lab_big, fill=TEXT + (255,))
    big_seat(*big)
    label("CUTOFF", big[0], 124, lab_mid, spacing=1.2)
    for cy, n in zip(COL_Y, ("RESO", "DRIVE", "MOD")):
        small_seat(col, cy, n)
lcd(LCD_A)
lcd(LCD_B)

# ---------- response screen ----------
inset(SCREEN, 12, fill=(7, 8, 8, 255), shadow=230)
x0, y0, x1, y1 = SCREEN
sw, sh_ = (x1 - x0 - 10) * S, (y1 - y0 - 10) * S
scr = np.zeros((sh_, sw, 3), np.float32)
syy, sxx = np.mgrid[0:sh_, 0:sw]
scr[:] = (8, 12, 11)
scr += (np.exp(-(((sxx - sw * 0.5) / (sw * 0.6)) ** 2 + ((syy - sh_ * 0.55) / (sh_ * 0.7)) ** 2)) * 10)[..., None] * np.array([0.2, 1.0, 0.65])
scr[::3] *= 0.82
scr += rng.normal(0, 1.0, scr.shape)
si = to_img(scr).convert("RGBA")
m = Image.new("L", si.size, 0)
ImageDraw.Draw(m).rounded_rectangle((0, 0, si.size[0] - 1, si.size[1] - 1), radius=8 * S, fill=255)
img.paste(si, ((x0 + 5) * S, (y0 + 5) * S), m)
d = BlendDraw(globals(), "img")
gl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
ImageDraw.Draw(gl).polygon([((x0 + 5) * S, (y0 + 5) * S), ((x0 + 150) * S, (y0 + 5) * S), ((x0 + 40) * S, (y1 - 5) * S), ((x0 + 5) * S, (y1 - 5) * S)],
                           fill=(255, 255, 255, 7))
img = Image.alpha_composite(img, gl)
d = BlendDraw(globals(), "img")
label("ROUTING", 430, 329, tiny, INK + (200,), spacing=1.0)

# ---------- bottom decks ----------
inset(LFO_DECK, 12, fill=(16, 17, 17, 255))
inset(MOD_DECK, 12, fill=(16, 17, 17, 255))
d.text((28 * S, 356 * S), "LFO", font=lab, fill=TEXT + (255,))
d.text((496 * S, 356 * S), "MODULATION", font=lab, fill=TEXT + (255,))
d.text((706 * S, 358 * S), "ADSR TRIGGER", font=tiny, fill=INK + (220,))

# step pattern well
inset(PATTERN, 6, fill=(8, 9, 9, 255), outline=(60, 70, 66, 160), shadow=230)
px0, py0, px1, py1 = PATTERN
for k in range(1, 16):
    x = px0 + 4 + (px1 - px0 - 8) * k / 16
    d.line((x * S, (py0 + 4) * S, x * S, (py1 - 4) * S), fill=ACC + (34 if k % 4 == 0 else 12,), width=1)
for k in range(1, 4):
    y = py0 + 4 + (py1 - py0 - 8) * k / 4
    d.line(((px0 + 4) * S, y * S, (px1 - 4) * S, y * S), fill=ACC + (10,), width=1)
label("draw steps  ·  double-click = on / off  ·  right-click = 0", 240, 506, tiny, INK + (180,))

for cx, n in zip(LFO_KX, ("RATE", "DEPTH")):
    small_seat(cx, ROW_Y[0], n)
for cx, n in zip(LFO_KX, ("STEREO", "GLIDE")):
    small_seat(cx, ROW_Y[1], n)
for cx, n in zip(MOD_KX, ("ENV AMT", "ENV SPEED", "SENS", "MIX", "OUTPUT")):
    small_seat(cx, ROW_Y[0], n)
for cx, n in zip(MOD_KX, ("ATTACK", "DECAY", "SUSTAIN", "RELEASE", "ADSR AMT")):
    small_seat(cx, ROW_Y[1], n)
for x in ((MOD_KX[1] + MOD_KX[2]) / 2, (MOD_KX[2] + MOD_KX[3]) / 2):
    d.line((x * S, 382 * S, x * S, 446 * S), fill=(44, 50, 47, 255), width=S)
d.line((((LFO_KX[0] + PATTERN[2]) / 2 + 2) * S, 380 * S, (((LFO_KX[0] + PATTERN[2]) / 2) + 2) * S, 516 * S), fill=(44, 50, 47, 255), width=S)

screw = render_knob(int(14 * S), "alu", seed=9)
sdr = ImageDraw.Draw(screw)
cs = screw.size[0] / 2
for a in (0.4, 0.4 + math.pi / 2):
    sdr.line((cs - math.cos(a) * cs * 0.38, cs - math.sin(a) * cs * 0.38, cs + math.cos(a) * cs * 0.38, cs + math.sin(a) * cs * 0.38), fill=(40, 40, 44, 255), width=max(2, S))
for (x, y) in ((10, 10), (BW - 10, 10), (10, BH - 10), (BW - 10, BH - 10)):
    img.alpha_composite(screw, (int(x * S - cs), int(y * S - cs)))

out = os.path.join(ROOT, "Resources", "bg.png")
img.convert("RGB").save(out, optimize=True)
print("wrote", out, img.size)

style = os.environ.get("SWEEP_KNOB", "black")
render_knob(272, style, seed=4).save(os.path.join(ROOT, "Resources", "knob_big.png"), optimize=True)
render_knob(112, style, seed=5).save(os.path.join(ROOT, "Resources", "knob_small.png"), optimize=True)
print("knobs:", style)
