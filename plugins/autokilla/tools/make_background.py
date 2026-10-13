"""Generates the TrapWin AUTO KILLA editor background (Resources/bg.png) at 2x of the 860x520 base size,
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
BW, BH = 860, 520
W, H = BW * S, BH * S
ROOT = os.path.dirname(HERE)
FONTS = os.path.join(ROOT, "Resources", "fonts")
rng = np.random.default_rng(47)
BLUE = (47, 123, 255)
ICE = (150, 190, 255)
TEXT = (226, 232, 242)
INK = (128, 138, 156)

# ---- layout (base px) ----
RETUNE_C = (111, 200)
SCREEN = (218, 96, 642, 322)
NOTE_PANEL = (654, 96, 844, 206)
RIGHT_PANEL = (654, 212, 844, 322)
LCD_RETUNE = (51, 292, 171, 320)
ROW2_L = (16, 330, 206, 372)
KEYS = (218, 330, 642, 372)
ROW2_R = (654, 330, 844, 372)
DECK = (16, 382, 844, 508)
SMALL_Y = 436
SMALL_X = [68 + 103.4 * i for i in range(8)]
SMALL_NAMES = ["HUMANIZE", "THRESHOLD", "VIBRATO", "VIB RATE", "TRANSPOSE", "THROAT", "MIX", "OUTPUT"]


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


# ---------- chassis: blue-black anodised with fine horizontal brushing + cool key light ----------
base = np.zeros((H, W, 3), np.float32)
base[:] = (21, 23, 29)
yy, xx = np.mgrid[0:H, 0:W]
brush = rng.normal(0, 1, (H, 1)) * 2.2 + rng.normal(0, 1, (H, W)) * 1.2
brush = np.asarray(Image.fromarray(np.uint8(np.clip(128 + brush * 6, 0, 255))).filter(ImageFilter.BoxBlur(1)), np.float32) - 128
base += brush[..., None] * 0.42
light = 1.0 + 0.18 * (1.0 - (xx / W) * 0.4 - (yy / H))
base *= light[..., None]
glow = np.exp(-(((xx - W * 0.5) / (W * 0.45)) ** 2 + ((yy - H * 0.32) / (H * 0.42)) ** 2))
base += glow[..., None] * np.array([2, 7, 18])
vig = 1.0 - 0.38 * (((xx - W / 2) / (W / 2)) ** 2 + ((yy - H / 2) / (H / 2)) ** 2) ** 1.5
base *= vig[..., None]
img = to_img(base).convert("RGBA")
d = BlendDraw(globals(), "img")


def rr(box, r, **kw):
    x0, y0, x1, y1 = box
    d.rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=r * S, **kw)


def inset(box, r=12, fill=(12, 13, 17, 255), outline=(70, 80, 100, 150), shadow=200):
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
    d.line(((x0 + r) * S, y1 * S + S, (x1 - r) * S, y1 * S + S), fill=(110, 120, 140, 70), width=S)


lab = font("BarlowCondensed-Bold.ttf", 15)
lab_big = font("BarlowCondensed-Bold.ttf", 18)
small = font("BarlowCondensed-Bold.ttf", 12)
tiny = font("BarlowCondensed-SemiBold.ttf", 11)


def label(text, cx, y, f=lab, col=TEXT + (255,), spacing=0.0, anchor="c"):
    widths = [d.textlength(ch, font=f) for ch in text]
    tw = sum(widths) + spacing * S * (len(text) - 1)
    x = cx * S - tw / 2 if anchor == "c" else cx * S
    for ch, w in zip(text, widths):
        d.text((x, y * S + S), ch, font=f, fill=(0, 0, 0, 170))
        d.text((x, y * S), ch, font=f, fill=col)
        x += w + spacing * S


# ---------- header: chrome wordmark + full official TrapWin logo ----------
img = place_title(img, chrome_title("AUTO KILLA", 30 * S, glow=BLUE, glow_strength=0.9), 24 * S, 14 * S)
_lg = trapwin_logo(36 * S)
img = place(img, _lg, 20 * S, 50 * S)
d = BlendDraw(globals(), "img")
d.text((20 * S + _lg.width + 12 * S, 62 * S), "REAL-TIME VOCAL TUNER  ·  HARD TUNE TO NATURAL", font=font("BarlowCondensed-SemiBold.ttf", 13), fill=INK + (255,))

hl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
hd = ImageDraw.Draw(hl)
for x in range(16 * S, (BW - 16) * S):
    t = (x / S - 16) / (BW - 32)
    a = int(100 * math.sin(math.pi * t) ** 2)
    hd.point((x, 86 * S), fill=BLUE + (a,))
img = Image.alpha_composite(img, hl)
d = BlendDraw(globals(), "img")


# ---------- big dial seat ----------
def big_seat(cx, cy, n_ticks=25, majors=(0, 6, 12, 18, 24), names=None):
    global img, d
    R = 68
    yy2, xx2 = np.mgrid[0:(2 * R + 8) * S, 0:(2 * R + 8) * S].astype(np.float32)
    cc = (R + 4) * S
    rr2 = np.sqrt((xx2 - cc) ** 2 + (yy2 - cc) ** 2) / S
    ang = np.arctan2(yy2 - cc, xx2 - cc)
    lip = (rr2 > R - 3) & (rr2 <= R + 2)
    floor = rr2 <= R - 3
    well = np.zeros(rr2.shape + (4,), np.float32)
    lit = 0.5 + 0.5 * np.cos(ang - math.radians(45))
    well[..., :3] = np.where(lip[..., None], (np.array([50, 54, 62]) + 100 * lit[..., None] ** 2), 0)
    shadowin = np.clip(1.0 - (R - 3 - rr2) / 16.0, 0, 1) * np.clip(0.5 - 0.5 * np.cos(ang - math.radians(45)), 0, 1)
    flo = np.array([11, 12, 15]) * (1.0 - 0.6 * shadowin[..., None])
    well[..., :3] = np.where(floor[..., None], flo, well[..., :3])
    well[..., 3] = np.where(lip | floor, 255, 0) * np.clip(R + 2 - rr2 + 0.5, 0, 1)
    img.alpha_composite(Image.fromarray(np.clip(well, 0, 255).astype(np.uint8), "RGBA"), (int((cx - R - 4) * S), int((cy - R - 4) * S)))
    d = BlendDraw(globals(), "img")
    d.arc(((cx - 61) * S, (cy - 61) * S, (cx + 61) * S, (cy + 61) * S), start=135, end=45, fill=(28, 32, 40, 255), width=5 * S)
    for i in range(n_ticks):
        a = math.radians(225 - 270 * i / (n_ticks - 1))
        major = i in majors
        r0, r1 = 74, (82 if major else 78)
        d.line(((cx + math.cos(a) * r0) * S, (cy - math.sin(a) * r0) * S, (cx + math.cos(a) * r1) * S, (cy - math.sin(a) * r1) * S),
               fill=(215, 225, 242, 220 if major else 90), width=S + (1 if major else 0))
    if names:
        for frac, t in names:
            a = math.radians(225 - 270 * frac)
            tx, ty = cx + math.cos(a) * 88, cy - math.sin(a) * 88
            tw = d.textlength(t, font=small)
            d.text((tx * S - tw / 2, (ty - 2) * S), t, font=small, fill=INK + (255,))


big_seat(*RETUNE_C, names=[(0.0, "HARD"), (1.0, "SLOW")])
label("RETUNE SPEED", RETUNE_C[0], 97, lab_big, spacing=1.5)


def lcd(box, r=6):
    """small blue-black LCD window (text painted live)"""
    global img, d
    inset(box, r, fill=(7, 8, 11, 255), outline=(60, 70, 90, 170), shadow=230)
    x0, y0, x1, y1 = box
    g = np.zeros((int((y1 - y0 - 4) * S), int((x1 - x0 - 4) * S), 4), np.float32)
    g[..., :3] = (5, 12, 26)
    g[::3, :, :3] *= 0.7
    g[..., 3] = 255
    gi = Image.fromarray(np.clip(g, 0, 255).astype(np.uint8), "RGBA")
    m = Image.new("L", gi.size, 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, gi.size[0] - 1, gi.size[1] - 1), radius=max(1, (r - 2)) * S, fill=255)
    img.paste(gi, (int((x0 + 2) * S), int((y0 + 2) * S)), m)
    d = BlendDraw(globals(), "img")


lcd(LCD_RETUNE)


def screen(box, r=12):
    global img, d
    inset(box, r, fill=(6, 7, 10, 255), shadow=230)
    x0, y0, x1, y1 = box
    sw, sh_ = (x1 - x0 - 10) * S, (y1 - y0 - 10) * S
    scr = np.zeros((sh_, sw, 3), np.float32)
    syy, sxx = np.mgrid[0:sh_, 0:sw]
    scr[:] = (6, 9, 16)
    scr += (np.exp(-(((sxx - sw * 0.5) / (sw * 0.6)) ** 2 + ((syy - sh_ * 0.6) / (sh_ * 0.7)) ** 2)) * 12)[..., None] * np.array([0.25, 0.5, 1.0])
    scr[::3] *= 0.85
    scr += rng.normal(0, 0.9, scr.shape)
    si = to_img(scr).convert("RGBA")
    m = Image.new("L", si.size, 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, si.size[0] - 1, si.size[1] - 1), radius=(r - 4) * S, fill=255)
    img.paste(si, ((x0 + 5) * S, (y0 + 5) * S), m)
    d = BlendDraw(globals(), "img")
    gl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(gl).polygon([((x0 + 5) * S, (y0 + 5) * S), ((x0 + 120) * S, (y0 + 5) * S), ((x0 + 40) * S, (y1 - 5) * S), ((x0 + 5) * S, (y1 - 5) * S)],
                               fill=(255, 255, 255, 7))
    img = Image.alpha_composite(img, gl)
    d = BlendDraw(globals(), "img")


screen(SCREEN)
# readout strip separator inside the pitch screen
d.line(((SCREEN[0] + 12) * S, 120 * S, (SCREEN[2] - 12) * S, 120 * S), fill=BLUE + (50,), width=S)

# note display
screen(NOTE_PANEL, 12)
label("NOTE", NOTE_PANEL[0] + 14, 101, tiny, INK + (220,), spacing=1.0, anchor="l")

# right panel: key / scale / auto key
inset(RIGHT_PANEL, 10, fill=(15, 16, 20, 255))
label("KEY", 664, 222, lab, TEXT + (230,), anchor="l")
label("SCALE", 664, 256, lab, TEXT + (230,), anchor="l")

# row 2 wells
inset(ROW2_L, 10, fill=(15, 16, 20, 255))
lcd((136, 337, 200, 365))
inset(KEYS, 8, fill=(9, 10, 13, 255), shadow=230)
inset(ROW2_R, 10, fill=(15, 16, 20, 255))

# bottom deck
inset(DECK, 12, fill=(15, 16, 20, 255))
for cx, t in zip(SMALL_X, SMALL_NAMES):
    label(t, cx, 389, lab)
    cy = SMALL_Y
    for k in range(21):
        a = math.radians(225 - 270 * k / 20)
        x, y = cx + math.cos(a) * 29, cy - math.sin(a) * 29
        d.ellipse(((x - 1.6) * S, (y - 1.6) * S, (x + 1.6) * S, (y + 1.6) * S), fill=(40, 44, 52, 255))
    d.ellipse(((cx - 24) * S, (cy - 24) * S, (cx + 24) * S, (cy + 24) * S), fill=(6, 7, 9, 255))

groups = [("NATURAL", 0, 1), ("VIBRATO", 2, 3), ("PITCH & THROAT", 4, 5), ("OUTPUT", 6, 7)]
for name, a, b in groups:
    xa, xb = SMALL_X[a] - 30, SMALL_X[b] + 30
    cx = (xa + xb) / 2
    tw = d.textlength(name, font=tiny) / S
    d.line((xa * S, 497 * S, (cx - tw / 2 - 6) * S, 497 * S), fill=BLUE + (80,), width=S)
    d.line(((cx + tw / 2 + 6) * S, 497 * S, xb * S, 497 * S), fill=BLUE + (80,), width=S)
    label(name, cx, 490, tiny, ICE + (200,))
for i in (1, 3, 5):
    x = (SMALL_X[i] + SMALL_X[i + 1]) / 2
    d.line((x * S, 392 * S, x * S, 484 * S), fill=(44, 48, 56, 255), width=S)

# screws
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

style = os.environ.get("AK_KNOB", "black")
render_knob(320, style, seed=4).save(os.path.join(ROOT, "Resources", "knob_big.png"), optimize=True)
render_knob(128, style, seed=5).save(os.path.join(ROOT, "Resources", "knob_small.png"), optimize=True)
print("knobs:", style)
