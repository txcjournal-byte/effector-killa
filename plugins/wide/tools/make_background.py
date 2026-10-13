"""Generates the TrapWin WIDE editor background (Resources/bg.png) at 2x of the 860x540 base size
and the photoreal knob image (Resources/knob_small.png).
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
rng = np.random.default_rng(23)
LAV = (185, 168, 255)
TEXT = (232, 230, 240)
INK = (140, 136, 156)

# ---- layout (base px) ----
SCOPE = (20, 100, 300, 380)
CORR_LCD = (20, 392, 300, 452)
PEAK_LCD = (20, 464, 300, 524)
BAND = (316, 100, 840, 262)
WROW_Y = 326
WIDTH_KNOBS = {"LOW WIDTH": 400, "MID WIDTH": 578, "HIGH WIDTH": 756}
XO_KNOBS = {"LOW X-OVER": 489, "HIGH X-OVER": 667}
DECK = (316, 386, 840, 524)
DECK_Y = 450
DECK_KNOBS = {"MONO BELOW": 372, "HAAS": 458, "BALANCE": 614, "STEREOIZE": 708, "OUTPUT": 796}
HAAS_BTNS = (500, 424, 540, 476)     # L / R buttons stacked
F_LO, F_HI = 20.0, 20000.0


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), int(size * S))


def to_img(a):
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))


class BlendDraw:
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


# ---------- chassis: cool graphite with a faint lavender sheen ----------
base = np.zeros((H, W, 3), np.float32)
base[:] = (22, 22, 27)
yy, xx = np.mgrid[0:H, 0:W]
brush = rng.normal(0, 1, (H, 1)) * 2.0 + rng.normal(0, 1, (H, W)) * 1.1
brush = np.asarray(Image.fromarray(np.uint8(np.clip(128 + brush * 6, 0, 255))).filter(ImageFilter.BoxBlur(1)), np.float32) - 128
base += brush[..., None] * 0.40
base *= (1.0 + 0.16 * (1.0 - (xx / W) * 0.3 - (yy / H)))[..., None]
glow = np.exp(-(((xx - W * 0.55) / (W * 0.5)) ** 2 + ((yy - H * 0.35) / (H * 0.5)) ** 2))
base += glow[..., None] * np.array([9, 7, 18])
vig = 1.0 - 0.38 * (((xx - W / 2) / (W / 2)) ** 2 + ((yy - H / 2) / (H / 2)) ** 2) ** 1.5
base *= vig[..., None]
img = to_img(base).convert("RGBA")
d = BlendDraw(globals(), "img")


def rr(box, r, **kw):
    x0, y0, x1, y1 = box
    d.rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=r * S, **kw)


def inset(box, r=12, fill=(13, 13, 16, 255), outline=(82, 78, 98, 150), shadow=200):
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
    d.line(((x0 + r) * S, y1 * S + S, (x1 - r) * S, y1 * S + S), fill=(116, 112, 136, 70), width=S)


lab = font("BarlowCondensed-Bold.ttf", 15)
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
    d.text((cx * S - tw / 2, y * S + S), text, font=f, fill=(0, 0, 0, 170))
    d.text((cx * S - tw / 2, y * S), text, font=f, fill=col)


def screen(box, r=12, tint=(10, 9, 16)):
    """recessed glass screen with scanlines; returns its inner offset"""
    global img, d
    inset(box, r, fill=(7, 7, 9, 255), shadow=230)
    x0, y0, x1, y1 = box
    sw, sh_ = (x1 - x0 - 10) * S, (y1 - y0 - 10) * S
    scr = np.zeros((sh_, sw, 3), np.float32)
    syy, sxx = np.mgrid[0:sh_, 0:sw]
    scr[:] = tint
    scr += (np.exp(-(((sxx - sw * 0.5) / (sw * 0.6)) ** 2 + ((syy - sh_ * 0.5) / (sh_ * 0.7)) ** 2)) * 9)[..., None] * np.array([0.6, 0.55, 1.0])
    scr[::3] *= 0.82
    scr += rng.normal(0, 0.9, scr.shape)
    si = to_img(scr).convert("RGBA")
    m = Image.new("L", si.size, 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, si.size[0] - 1, si.size[1] - 1), radius=(r - 4) * S, fill=255)
    img.paste(si, ((x0 + 5) * S, (y0 + 5) * S), m)
    d = BlendDraw(globals(), "img")


def glass(box):
    global img, d
    x0, y0, x1, y1 = box
    gl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(gl).polygon([((x0 + 5) * S, (y0 + 5) * S), ((x0 + (x1 - x0) * 0.45) * S, (y0 + 5) * S),
                                ((x0 + (x1 - x0) * 0.18) * S, (y1 - 5) * S), ((x0 + 5) * S, (y1 - 5) * S)], fill=(255, 255, 255, 8))
    img = Image.alpha_composite(img, gl)
    d = BlendDraw(globals(), "img")


# ---------- header ----------
img = place_title(img, chrome_title("WIDE", 30 * S, glow=LAV, glow_strength=0.9), 24 * S, 14 * S)
_lg = trapwin_logo(36 * S)
img = place(img, _lg, 20 * S, 50 * S)
d = BlendDraw(globals(), "img")
d.text((20 * S + _lg.width + 12 * S, 62 * S), "STEREO IMAGER  ·  3-BAND WIDTH  ·  MONO BELOW  ·  HAAS", font=font("BarlowCondensed-SemiBold.ttf", 13), fill=INK + (255,))
hl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
hd = ImageDraw.Draw(hl)
for x in range(16 * S, (BW - 16) * S):
    t = (x / S - 16) / (BW - 32)
    hd.point((x, 86 * S), fill=LAV + (int(95 * math.sin(math.pi * t) ** 2),))
img = Image.alpha_composite(img, hl)
d = BlendDraw(globals(), "img")

# ---------- vectorscope screen + graticule ----------
screen(SCOPE)
x0, y0, x1, y1 = SCOPE
cx, cy = (x0 + x1) / 2, (y0 + y1) / 2 - 2
R = 108
for frac, a in ((1.0, 60), (0.5, 26)):
    d.ellipse(((cx - R * frac) * S, (cy - R * frac) * S, (cx + R * frac) * S, (cy + R * frac) * S), outline=LAV + (a,), width=S)
d.line(((cx - R) * S, cy * S, (cx + R) * S, cy * S), fill=LAV + (40,), width=S)
d.line((cx * S, (cy - R) * S, cx * S, (cy + R) * S), fill=LAV + (60,), width=S)
for ang in (45, 135):
    a = math.radians(ang)
    d.line(((cx - math.cos(a) * R) * S, (cy + math.sin(a) * R) * S, (cx + math.cos(a) * R) * S, (cy - math.sin(a) * R) * S), fill=LAV + (34,), width=S)
for ang, t in ((90, "M"), (135, "L"), (45, "R")):
    a = math.radians(ang)
    tx, ty = cx + math.cos(a) * (R + 10), cy - math.sin(a) * (R + 10)
    tw = d.textlength(t, font=small)
    d.text((tx * S - tw / 2, (ty - 8) * S), t, font=small, fill=LAV + (170,))
for t, tx in (("+S", cx - R - 6), ("-S", cx + R + 6)):
    tw = d.textlength(t, font=tiny)
    d.text((tx * S - tw / 2, (cy - 18) * S), t, font=tiny, fill=LAV + (120,))
glass(SCOPE)
label("VECTORSCOPE", x0 + 50, y0 + 12, tiny, INK + (200,), spacing=1.2)

# ---------- correlation + peak LCDs ----------
screen(CORR_LCD, 8)
glass(CORR_LCD)
screen(PEAK_LCD, 8)
glass(PEAK_LCD)

# ---------- band display ----------
screen(BAND)
bx0, by0, bx1, by1 = BAND[0] + 12, BAND[1] + 12, BAND[2] - 12, BAND[3] - 24


def fx(f):
    return bx0 + (bx1 - bx0) * math.log(f / F_LO) / math.log(F_HI / F_LO)


for f in (30, 40, 50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000):
    major = f in (100, 1000, 10000)
    d.line((fx(f) * S, by0 * S, fx(f) * S, by1 * S), fill=LAV + (34 if major else 14,), width=S)
for f, t in ((50, "50"), (100, "100"), (200, "200"), (500, "500"), (1000, "1k"), (2000, "2k"), (5000, "5k"), (10000, "10k")):
    tw = d.textlength(t, font=tiny)
    d.text((fx(f) * S - tw / 2, (by1 + 4) * S), t, font=tiny, fill=INK + (190,))
mid_y = (by0 + by1) / 2
d.line((bx0 * S, mid_y * S, bx1 * S, mid_y * S), fill=LAV + (40,), width=S)
glass(BAND)

# ---------- width row ----------
def knob_seat(cx, cy, r_dots, r_well, n=21):
    for k in range(n):
        a = math.radians(225 - 270 * k / (n - 1))
        x, y = cx + math.cos(a) * r_dots, cy - math.sin(a) * r_dots
        d.ellipse(((x - 1.6) * S, (y - 1.6) * S, (x + 1.6) * S, (y + 1.6) * S), fill=(44, 42, 54, 255))
    d.ellipse(((cx - r_well) * S, (cy - r_well) * S, (cx + r_well) * S, (cy + r_well) * S), fill=(6, 6, 8, 255))


for name, x in WIDTH_KNOBS.items():
    label(name, x, WROW_Y - 54, lab, spacing=0.6)
    knob_seat(x, WROW_Y, 36, 30)
for name, x in XO_KNOBS.items():
    label(name, x, WROW_Y - 44, small, INK + (230,), spacing=0.5)
    knob_seat(x, WROW_Y, 25, 20, n=17)

# ---------- bottom deck ----------
inset(DECK, 12, fill=(16, 16, 19, 255))
for name, x in DECK_KNOBS.items():
    label(name, x, DECK[1] + 10, lab, spacing=0.4)
    knob_seat(x, DECK_Y, 29, 24)
for sx in (415, 577, 661, 752):
    d.line((sx * S, (DECK[1] + 12) * S, sx * S, (DECK[3] - 12) * S), fill=(48, 46, 58, 255), width=S)

label("MID", DECK_KNOBS["BALANCE"] - 27, DECK_Y + 24, tiny, INK + (200,))
label("SIDE", DECK_KNOBS["BALANCE"] + 28, DECK_Y + 24, tiny, INK + (200,))

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
render_knob(176, "alu", seed=5).save(os.path.join(ROOT, "Resources", "knob_small.png"), optimize=True)
print("knob done")
