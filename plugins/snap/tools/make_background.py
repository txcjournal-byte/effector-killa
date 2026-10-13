"""Generates the TrapWin SNAP editor background (Resources/bg.png) at 2x of the 840x520 base size,
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
BW, BH = 840, 520
W, H = BW * S, BH * S
ROOT = os.path.dirname(HERE)
FONTS = os.path.join(ROOT, "Resources", "fonts")
rng = np.random.default_rng(31)
ACC = (255, 61, 127)
TEXT = (236, 230, 220)
INK = (146, 138, 126)

# ---- layout (base px) ----
ATT_C = (132, 208)
SUS_C = (708, 208)
SCREEN = (256, 96, 584, 292)
LCD_W = 120
LCD_Y = (310, 340)
DECK_L = (16, 358, 226, 506)
DECK_R = (238, 358, 824, 506)
LOOK_BTN = (32, 372, 116, 452)
CLIP_BTN = (126, 372, 210, 452)
SMALL_Y = 424
SMALL_X = [302 + 114 * i for i in range(5)]
SMALL_NAMES = ["SPLIT", "SPEED", "CEILING", "MIX", "OUTPUT"]
# screen plot areas (base px, panel coordinates) - must match SnapDisplay in PluginEditor.cpp
ENV_AREA = (272, 122, 568, 222)
GAIN_AREA = (272, 230, 568, 272)


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
base += glow[..., None] * np.array([16, 4, 8])
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




# ---------- header: chrome wordmark + full official TrapWin logo ----------
img = place_title(img, chrome_title("SNAP", 30 * S, glow=ACC, glow_strength=0.85), 24 * S, 14 * S)
_lg = trapwin_logo(36 * S)
img = place(img, _lg, 20 * S, 50 * S)
d = BlendDraw(globals(), "img")
d.text((20 * S + _lg.width + 12 * S, 62 * S), "TRANSIENT SHAPER  ·  SPLIT BAND  ·  LOOKAHEAD", font=font("BarlowCondensed-SemiBold.ttf", 13), fill=INK + (255,))

hl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
hd = ImageDraw.Draw(hl)
for x in range(16 * S, (BW - 16) * S):
    t = (x / S - 16) / (BW - 32)
    a = int(90 * math.sin(math.pi * t) ** 2)
    hd.point((x, 86 * S), fill=ACC + (a,))
img = Image.alpha_composite(img, hl)
d = BlendDraw(globals(), "img")


# ---------- big dial seats (bipolar: centre detent at the top) ----------
def big_seat(cx, cy, names):
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
    well[..., :3] = np.where(lip[..., None], (np.array([56, 52, 50]) + 100 * lit[..., None] ** 2), 0)
    shadowin = np.clip(1.0 - (R - 3 - rr2) / 16.0, 0, 1) * np.clip(0.5 - 0.5 * np.cos(ang - math.radians(45)), 0, 1)
    flo = np.array([13, 12, 13]) * (1.0 - 0.6 * shadowin[..., None])
    well[..., :3] = np.where(floor[..., None], flo, well[..., :3])
    well[..., 3] = np.where(lip | floor, 255, 0) * np.clip(R + 2 - rr2 + 0.5, 0, 1)
    img.alpha_composite(Image.fromarray(np.clip(well, 0, 255).astype(np.uint8), "RGBA"), (int((cx - R - 4) * S), int((cy - R - 4) * S)))
    d = BlendDraw(globals(), "img")
    d.arc(((cx - 61) * S, (cy - 61) * S, (cx + 61) * S, (cy + 61) * S), start=135, end=45, fill=(36, 30, 33, 255), width=5 * S)
    n_ticks = 21
    for i in range(n_ticks):
        a = math.radians(225 - 270 * i / (n_ticks - 1))
        major = i in (0, 5, 10, 15, 20)
        r0, r1 = 74, (83 if major else 78)
        col = ACC + (235,) if i == 10 else (232, 222, 215, 220 if major else 90)
        d.line(((cx + math.cos(a) * r0) * S, (cy - math.sin(a) * r0) * S, (cx + math.cos(a) * r1) * S, (cy - math.sin(a) * r1) * S),
               fill=col, width=S + (1 if major else 0))
    for frac, t in names:
        a = math.radians(225 - 270 * frac)
        tx, ty = cx + math.cos(a) * 95, cy - math.sin(a) * 95
        tw = d.textlength(t, font=small)
        d.text((tx * S - tw / 2, (ty - 7) * S), t, font=small, fill=INK + (255,))


big_seat(*ATT_C, names=[(0.0, "SOFT"), (1.0, "PUNCH")])
big_seat(*SUS_C, names=[(0.0, "TIGHT"), (1.0, "LONG")])
label("ATTACK", ATT_C[0], 94, lab_big, spacing=1.5)
label("SUSTAIN", SUS_C[0], 94, lab_big, spacing=1.5)


def lcd(box):
    global img, d
    inset(box, 6, fill=(9, 8, 8, 255), outline=(70, 60, 64, 170), shadow=230)
    x0, y0, x1, y1 = box
    g = np.zeros(((y1 - y0 - 4) * S, (x1 - x0 - 4) * S, 4), np.float32)
    g[..., :3] = (24, 7, 13)
    g[::3, :, :3] *= 0.7
    g[..., 3] = 255
    gi = Image.fromarray(np.clip(g, 0, 255).astype(np.uint8), "RGBA")
    m = Image.new("L", gi.size, 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, gi.size[0] - 1, gi.size[1] - 1), radius=4 * S, fill=255)
    img.paste(gi, ((x0 + 2) * S, (y0 + 2) * S), m)
    d = BlendDraw(globals(), "img")


lcd((ATT_C[0] - LCD_W // 2, LCD_Y[0], ATT_C[0] + LCD_W // 2, LCD_Y[1]))
lcd((SUS_C[0] - LCD_W // 2, LCD_Y[0], SUS_C[0] + LCD_W // 2, LCD_Y[1]))

# ---------- envelope screen ----------
inset(SCREEN, 12, fill=(8, 7, 8, 255), shadow=230)
x0, y0, x1, y1 = SCREEN
sw, sh_ = (x1 - x0 - 10) * S, (y1 - y0 - 10) * S
scr = np.zeros((sh_, sw, 3), np.float32)
syy, sxx = np.mgrid[0:sh_, 0:sw]
scr[:] = (13, 9, 11)
scr += (np.exp(-(((sxx - sw * 0.5) / (sw * 0.6)) ** 2 + ((syy - sh_ * 0.5) / (sh_ * 0.7)) ** 2)) * 10)[..., None] * np.array([1.0, 0.25, 0.5])
scr[::3] *= 0.82
scr += rng.normal(0, 1.0, scr.shape)
si = to_img(scr).convert("RGBA")
_SI = {"si": si}
sd = BlendDraw(_SI, "si")
ox, oy = x0 + 5, y0 + 5


def sbox(a):
    return ((a[0] - ox) * S, (a[1] - oy) * S, (a[2] - ox) * S, (a[3] - oy) * S)


ex0, ey0, ex1, ey1 = sbox(ENV_AREA)
for k in range(9):
    gx = ex0 + (ex1 - ex0) * k / 8
    sd.line((gx, ey0, gx, ey1), fill=ACC + (14 if k % 2 else 26,), width=1)
for k in range(5):                    # dB grid: 0 / -12 / -24 / -36 / -48
    gy = ey0 + (ey1 - ey0) * k / 4
    sd.line((ex0, gy, ex1, gy), fill=ACC + (22 if k == 0 else 12,), width=1)
gx0, gy0, gx1, gy1 = sbox(GAIN_AREA)
for k in range(9):
    gx = gx0 + (gx1 - gx0) * k / 8
    sd.line((gx, gy0, gx, gy1), fill=ACC + (10,), width=1)
sd.line((gx0, (gy0 + gy1) / 2, gx1, (gy0 + gy1) / 2), fill=ACC + (55,), width=S)
sd.line((gx0, gy0 - 4 * S, gx1, gy0 - 4 * S), fill=ACC + (26,), width=1)
m = Image.new("L", si.size, 0)
ImageDraw.Draw(m).rounded_rectangle((0, 0, si.size[0] - 1, si.size[1] - 1), radius=8 * S, fill=255)
si = _SI["si"]
img.paste(si, (ox * S, oy * S), m)
d = BlendDraw(globals(), "img")
d.line(((x0 + 14) * S, 117 * S, (x1 - 14) * S, 117 * S), fill=ACC + (40,), width=S)
gl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
ImageDraw.Draw(gl).polygon([((x0 + 5) * S, (y0 + 5) * S), ((x0 + 150) * S, (y0 + 5) * S), ((x0 + 40) * S, (y1 - 5) * S), ((x0 + 5) * S, (y1 - 5) * S)],
                           fill=(255, 255, 255, 8))
img = Image.alpha_composite(img, gl)
d = BlendDraw(globals(), "img")

label("SHAPE BAND", (SCREEN[0] + SCREEN[2]) / 2, 336, tiny, INK + (200,), spacing=1.0)

# ---------- bottom deck ----------
inset(DECK_L, 12, fill=(16, 15, 16, 255))
inset(DECK_R, 12, fill=(16, 15, 16, 255))


def bezel(box, r=10):
    global img, d
    x0, y0, x1, y1 = box
    rr((x0 - 4, y0 - 4, x1 + 4, y1 + 4), r + 3, fill=(8, 8, 9, 255))
    grad = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(grad)
    gd.rounded_rectangle(((x0 - 4) * S, (y0 - 4) * S, (x1 + 4) * S, (y1 + 4) * S), radius=(r + 3) * S, outline=(120, 110, 112, 120), width=S)
    gd.rounded_rectangle(((x0 - 2) * S, (y0 - 2) * S, (x1 + 2) * S, (y1 + 2) * S), radius=(r + 1) * S, outline=(0, 0, 0, 220), width=2 * S)
    img = Image.alpha_composite(img, grad)
    d = BlendDraw(globals(), "img")


bezel(LOOK_BTN)
bezel(CLIP_BTN)
label("lookahead = 4 ms latency (reported)", (DECK_L[0] + DECK_L[2]) / 2, 466, tiny, INK + (190,))
label("clip = soft ceiling after shaping", (DECK_L[0] + DECK_L[2]) / 2, 482, tiny, INK + (190,))

for cx, t in zip(SMALL_X, SMALL_NAMES):
    label(t, cx, 363, lab)
    cy = SMALL_Y
    for k in range(21):
        a = math.radians(225 - 270 * k / 20)
        x, y = cx + math.cos(a) * 29, cy - math.sin(a) * 29
        d.ellipse(((x - 1.6) * S, (y - 1.6) * S, (x + 1.6) * S, (y + 1.6) * S), fill=(46, 40, 43, 255))
    d.ellipse(((cx - 24) * S, (cy - 24) * S, (cx + 24) * S, (cy + 24) * S), fill=(7, 7, 8, 255))

groups = [("DETECTOR", 0, 1), ("CLIPPER", 2, 2), ("OUTPUT", 3, 4)]
for name, a, b in groups:
    xa, xb = SMALL_X[a] - 44, SMALL_X[b] + 44
    cx = (xa + xb) / 2
    tw = d.textlength(name, font=tiny) / S
    d.line((xa * S, 492 * S, (cx - tw / 2 - 6) * S, 492 * S), fill=ACC + (70,), width=S)
    d.line(((cx + tw / 2 + 6) * S, 492 * S, xb * S, 492 * S), fill=ACC + (70,), width=S)
    label(name, cx, 485, tiny, ACC + (200,))
for i in (1, 2):
    x = (SMALL_X[i] + SMALL_X[i + 1]) / 2
    d.line((x * S, 368 * S, x * S, 480 * S), fill=(50, 44, 47, 255), width=S)

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

style = os.environ.get("SNAP_KNOB", "black")
render_knob(320, style, seed=4).save(os.path.join(ROOT, "Resources", "knob_big.png"), optimize=True)
render_knob(128, style, seed=5).save(os.path.join(ROOT, "Resources", "knob_small.png"), optimize=True)
print("knobs:", style)
