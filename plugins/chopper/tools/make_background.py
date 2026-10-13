"""Generates the TrapWin CHOPPER editor background (Resources/bg.png) at 2x of the 820x500 base size,
plus the photoreal knob image (Resources/knob_med.png).
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
BW, BH = 820, 500
W, H = BW * S, BH * S
ROOT = os.path.dirname(HERE)
FONTS = os.path.join(ROOT, "Resources", "fonts")
rng = np.random.default_rng(47)
VIOLET = (155, 92, 255)
TEXT = (232, 230, 240)
INK = (138, 134, 156)

# ---- layout (base px) ----
SCREEN = (16, 96, 804, 290)
CELL_X0, CELL_X1 = 32, 788
CELL_Y0, CELL_Y1 = 142, 256
LED_Y = 132
NUM_Y = 262
PALETTE_Y = (302, 332)
PAL_X0, PAL_W, PAL_GAP = 16, 70, 5
RATE_X0, RATE_W, RATE_GAP = 590, 50, 4
DECK_L = (16, 352, 200, 488)
DECK_R = (212, 352, 804, 488)
HOLD_BTN = (34, 366, 182, 452)
KNOB_Y = 414
KNOB_X = [212 + 592 * (2 * i + 1) / 8 for i in range(4)]
KNOB_NAMES = ["REPEAT SIZE", "SWING", "MIX", "OUTPUT"]
LCD_W, LCD_Y = 96, (452, 474)


def cell_x(i):
    """left edge + width of step cell i (4 groups of 4, wider gap between groups)"""
    gap, ggap = 6.0, 14.0
    w = (CELL_X1 - CELL_X0 - 12 * gap - 3 * ggap) / 16.0
    x = CELL_X0 + i * (w + gap) + (i // 4) * (ggap - gap)
    return x, w


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


# ---------- chassis: dark blued steel, fine horizontal brushing, cool key light + violet bloom ----------
base = np.zeros((H, W, 3), np.float32)
base[:] = (23, 23, 29)
yy, xx = np.mgrid[0:H, 0:W]
brush = rng.normal(0, 1, (H, 1)) * 2.4 + rng.normal(0, 1, (H, W)) * 1.1
brush = np.asarray(Image.fromarray(np.uint8(np.clip(128 + brush * 6, 0, 255))).filter(ImageFilter.BoxBlur(1)), np.float32) - 128
base += brush[..., None] * 0.45
light = 1.0 + 0.20 * (1.0 - (xx / W) * 0.35 - (yy / H))
base *= light[..., None]
glow = np.exp(-(((xx - W * 0.5) / (W * 0.50)) ** 2 + ((yy - H * 0.36) / (H * 0.40)) ** 2))
base += glow[..., None] * np.array([9, 4, 16])
vig = 1.0 - 0.40 * (((xx - W / 2) / (W / 2)) ** 2 + ((yy - H / 2) / (H / 2)) ** 2) ** 1.5
base *= vig[..., None]
img = to_img(base).convert("RGBA")
d = BlendDraw(globals(), "img")


def rr(box, r, **kw):
    x0, y0, x1, y1 = box
    d.rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=r * S, **kw)


def inset(box, r=12, fill=(13, 13, 17, 255), outline=(78, 74, 96, 150), shadow=200):
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
    d.line(((x0 + r) * S, y1 * S + S, (x1 - r) * S, y1 * S + S), fill=(116, 112, 136, 70), width=S)


lab = font("BarlowCondensed-Bold.ttf", 15)
lab_big = font("BarlowCondensed-Bold.ttf", 16)
small = font("BarlowCondensed-Bold.ttf", 12)
tiny = font("BarlowCondensed-SemiBold.ttf", 11)
mono = font("ShareTechMono-Regular.ttf", 11)


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
img = place_title(img, chrome_title("CHOPPER", 30 * S, glow=VIOLET, glow_strength=0.95), 24 * S, 14 * S)
_lg = trapwin_logo(36 * S)
img = place(img, _lg, 20 * S, 50 * S)
d = BlendDraw(globals(), "img")
d.text((20 * S + _lg.width + 12 * S, 62 * S), "STUTTER  ·  GLITCH  ·  SLICE REPEAT", font=font("BarlowCondensed-SemiBold.ttf", 13), fill=INK + (255,))
label("PATTERN", 600, 66, tiny, INK + (210,), spacing=1.0)

# thin violet hairline under the header
hl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
hd = ImageDraw.Draw(hl)
for x in range(16 * S, (BW - 16) * S):
    t = (x / S - 16) / (BW - 32)
    a = int(110 * math.sin(math.pi * t) ** 2)
    hd.point((x, 89 * S), fill=VIOLET + (a,))
img = Image.alpha_composite(img, hl)
d = BlendDraw(globals(), "img")

# ---------- step sequencer screen ----------
inset(SCREEN, 12, fill=(7, 7, 10, 255), shadow=235)
x0, y0, x1, y1 = SCREEN
sw, sh_ = (x1 - x0 - 10) * S, (y1 - y0 - 10) * S
scr = np.zeros((sh_, sw, 3), np.float32)
syy, sxx = np.mgrid[0:sh_, 0:sw]
scr[:] = (11, 10, 16)
scr += (np.exp(-(((sxx - sw * 0.5) / (sw * 0.55)) ** 2 + ((syy - sh_ * 0.65) / (sh_ * 0.75)) ** 2)) * 12)[..., None] * np.array([0.65, 0.4, 1.0])
scr[::3] *= 0.84
scr += rng.normal(0, 0.9, scr.shape)
si = to_img(scr).convert("RGBA")
_SI = {"si": si}
sd = BlendDraw(_SI, "si")
ox, oy = (x0 + 5), (y0 + 5)


def sx(v):
    return (v - ox) * S


def sy(v):
    return (v - oy) * S


# readout strip separator
sd.line((sx(30), sy(124), sx(790), sy(124)), fill=VIOLET + (46,), width=S)
# beat-group brackets + cell wells + LED sockets
for g in range(4):
    gx0, _ = cell_x(g * 4)
    gx1, w = cell_x(g * 4 + 3)
    gx1 += w
    sd.rounded_rectangle((sx(gx0 - 4), sy(CELL_Y0 - 4), sx(gx1 + 4), sy(CELL_Y1 + 4)), radius=7 * S, fill=(255, 255, 255, 6),
                         outline=VIOLET + (30,), width=S)
for i in range(16):
    cx_, w = cell_x(i)
    sd.rounded_rectangle((sx(cx_), sy(CELL_Y0), sx(cx_ + w), sy(CELL_Y1)), radius=5 * S, fill=(0, 0, 0, 120))
    lx = cx_ + w / 2
    sd.ellipse((sx(lx - 3.2), sy(LED_Y - 3.2), sx(lx + 3.2), sy(LED_Y + 3.2)), fill=(28, 24, 38, 255), outline=(0, 0, 0, 200), width=S)
m = Image.new("L", si.size, 0)
ImageDraw.Draw(m).rounded_rectangle((0, 0, si.size[0] - 1, si.size[1] - 1), radius=8 * S, fill=255)
si = _SI["si"]
img.paste(si, (ox * S, oy * S), m)
d = BlendDraw(globals(), "img")
# step numbers (beat starts brighter)
for i in range(16):
    cx_, w = cell_x(i)
    t = str(i + 1)
    col = (196, 170, 255, 235) if i % 4 == 0 else (110, 104, 132, 220)
    tw = d.textlength(t, font=mono)
    d.text(((cx_ + w / 2) * S - tw / 2, NUM_Y * S), t, font=mono, fill=col)
# glass reflection
gl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
ImageDraw.Draw(gl).polygon([((x0 + 5) * S, (y0 + 5) * S), ((x0 + 240) * S, (y0 + 5) * S), ((x0 + 120) * S, (y1 - 5) * S), ((x0 + 5) * S, (y1 - 5) * S)],
                           fill=(255, 255, 255, 7))
img = Image.alpha_composite(img, gl)
d = BlendDraw(globals(), "img")

# ---------- palette + rate captions ----------
pal_x1 = PAL_X0 + 7 * PAL_W + 6 * PAL_GAP
label("PAINT EFFECT   ·   click a step to cycle   ·   right-click / drag to paint", (PAL_X0 + pal_x1) / 2, 336, tiny, INK + (205,))
rate_x1 = RATE_X0 + 4 * RATE_W + 3 * RATE_GAP
label("STEP RATE", (RATE_X0 + rate_x1) / 2, 336, tiny, INK + (205,), spacing=1.0)

# ---------- bottom deck ----------
inset(DECK_L, 12, fill=(15, 15, 19, 255))
inset(DECK_R, 12, fill=(15, 15, 19, 255))


def bezel(box, r=10):
    """machined bezel the illuminated button sits in"""
    global img, d
    x0, y0, x1, y1 = box
    rr((x0 - 4, y0 - 4, x1 + 4, y1 + 4), r + 3, fill=(7, 7, 9, 255))
    grad = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(grad)
    gd.rounded_rectangle(((x0 - 4) * S, (y0 - 4) * S, (x1 + 4) * S, (y1 + 4) * S), radius=(r + 3) * S, outline=(120, 114, 140, 120), width=S)
    gd.rounded_rectangle(((x0 - 2) * S, (y0 - 2) * S, (x1 + 2) * S, (y1 + 2) * S), radius=(r + 1) * S, outline=(0, 0, 0, 220), width=2 * S)
    img = Image.alpha_composite(img, grad)
    d = BlendDraw(globals(), "img")


bezel(HOLD_BTN)
label("hold = repeat now  ·  right-click = latch", (DECK_L[0] + DECK_L[2]) / 2, 466, tiny, INK + (195,))


def lcd(box):
    """small violet LCD window (text painted live)"""
    global img, d
    inset(box, 5, fill=(8, 7, 11, 255), outline=(70, 64, 90, 170), shadow=230)
    x0, y0, x1, y1 = box
    g = np.zeros(((y1 - y0 - 4) * S, (x1 - x0 - 4) * S, 4), np.float32)
    g[..., :3] = (17, 11, 28)
    g[::3, :, :3] *= 0.7
    g[..., 3] = 255
    gi = Image.fromarray(np.clip(g, 0, 255).astype(np.uint8), "RGBA")
    m = Image.new("L", gi.size, 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, gi.size[0] - 1, gi.size[1] - 1), radius=3 * S, fill=255)
    img.paste(gi, ((x0 + 2) * S, (y0 + 2) * S), m)
    d = BlendDraw(globals(), "img")


for i, (cx, t) in enumerate(zip(KNOB_X, KNOB_NAMES)):
    label(t, cx, 358, lab, spacing=0.6)
    cy = KNOB_Y
    n = 6 if i == 0 else 25
    for k in range(n):
        a = math.radians(225 - 270 * k / (n - 1))
        major = i == 0 or k % 6 == 0
        r0, r1 = 34, (39 if major else 36.5)
        d.line(((cx + math.cos(a) * r0) * S, (cy - math.sin(a) * r0) * S, (cx + math.cos(a) * r1) * S, (cy - math.sin(a) * r1) * S),
               fill=(220, 214, 240, 200 if major else 80), width=S + (1 if major else 0))
    # unlit arc groove (the editor paints the lit violet arc on top, radius 30)
    d.arc(((cx - 30) * S, (cy - 30) * S, (cx + 30) * S, (cy + 30) * S), start=135, end=45, fill=(34, 30, 44, 255), width=4 * S)
    d.ellipse(((cx - 26) * S, (cy - 26) * S, (cx + 26) * S, (cy + 26) * S), fill=(6, 6, 8, 255))
    lcd((int(cx - LCD_W / 2), LCD_Y[0], int(cx + LCD_W / 2), LCD_Y[1]))
    if i == 0:
        # stepped size legend
        for k, t2 in ((0, "1/4"), (5, "1/16T")):
            a = math.radians(225 - 270 * k / 5)
            tx, ty = cx + math.cos(a) * 40 + (-12 if k == 0 else 14), cy - math.sin(a) * 40 + 3
            tw = d.textlength(t2, font=tiny)
            d.text((tx * S - tw / 2, (ty - 6) * S), t2, font=tiny, fill=INK + (255,))

x = (KNOB_X[1] + KNOB_X[2]) / 2
d.line((x * S, 366 * S, x * S, 474 * S), fill=(46, 44, 56, 255), width=S)

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

# ---------- knob (static image; the editor draws the rotating pointer) ----------
render_knob(168, os.environ.get("CHOPPER_KNOB", "black"), seed=4).save(os.path.join(ROOT, "Resources", "knob_med.png"), optimize=True)
print("knob ok")
