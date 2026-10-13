"""Generates the TrapWin AURA VERB editor background (Resources/bg.png) at 2x of the 860x540 base size,
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
rng = np.random.default_rng(47)
GOLD = (232, 196, 106)
TEXT = (234, 230, 220)
INK = (138, 148, 170)

# ---- layout (base px) ----
DECAY_C = (130, 232)
MIX_C = (730, 232)
SCREEN = (252, 128, 608, 302)
LCD_W = 120
LCD_Y = (314, 338)
MODE_STRIP = (14, 90, 846, 124)
DECK_L = (16, 346, 196, 530)
DECK_R = (208, 346, 844, 530)
FREEZE_BTN = (28, 358, 184, 432)
ROW1_Y, ROW2_Y = 392, 478
ROW1_X = [252 + 90 * i for i in range(7)]
ROW2_X = [297 + 90 * i for i in range(6)]
ROW1_NAMES = ["PRE-DELAY", "SIZE", "EARLY / LATE", "ATTACK", "DIFFUSION", "MOD DEPTH", "MOD RATE"]
ROW2_NAMES = ["DAMPING", "LOW CUT", "BASS X", "WIDTH", "DUCK", "OUTPUT"]
GRAPH = (272, 154, 596, 276)     # time / level plot inside the screen (0 dB .. -60 dB)


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


# ---------- chassis: midnight-blue anodised metal, fine brushing, warm key light ----------
base = np.zeros((H, W, 3), np.float32)
base[:] = (15, 21, 40)
yy, xx = np.mgrid[0:H, 0:W]
brush = rng.normal(0, 1, (H, 1)) * 2.0 + rng.normal(0, 1, (H, W)) * 1.1
brush = np.asarray(Image.fromarray(np.uint8(np.clip(128 + brush * 6, 0, 255))).filter(ImageFilter.BoxBlur(1)), np.float32) - 128
base += brush[..., None] * np.array([0.30, 0.34, 0.46])
light = 1.0 + 0.22 * (1.0 - (xx / W) * 0.4 - (yy / H))
base *= light[..., None]
glow = np.exp(-(((xx - W * 0.5) / (W * 0.42)) ** 2 + ((yy - H * 0.36) / (H * 0.40)) ** 2))
base += glow[..., None] * np.array([12, 11, 9])
# faint starfield sparkle in the chassis (very subtle, it's a "space" product)
stars = rng.random((H, W)) > 0.99965
base[stars] += np.array([40, 38, 30]) * rng.random((stars.sum(), 1))
vig = 1.0 - 0.42 * (((xx - W / 2) / (W / 2)) ** 2 + ((yy - H / 2) / (H / 2)) ** 2) ** 1.5
base *= vig[..., None]
img = to_img(base).convert("RGBA")
d = BlendDraw(globals(), "img")


def rr(box, r, **kw):
    x0, y0, x1, y1 = box
    d.rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=r * S, **kw)


def inset(box, r=12, fill=(8, 11, 22, 255), outline=(96, 104, 128, 140), shadow=210):
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
    d.line(((x0 + r) * S, y1 * S + S, (x1 - r) * S, y1 * S + S), fill=(140, 150, 175, 60), width=S)


lab = font("BarlowCondensed-Bold.ttf", 14)
lab_big = font("BarlowCondensed-Bold.ttf", 18)
small = font("BarlowCondensed-Bold.ttf", 12)
tiny = font("BarlowCondensed-SemiBold.ttf", 11)


def label(text, cx, y, f=lab, col=TEXT + (255,), spacing=0.0):
    widths = [d.textlength(ch, font=f) for ch in text]
    tw = sum(widths) + spacing * S * (len(text) - 1)
    x = cx * S - tw / 2
    if not spacing:
        d.text((x, y * S + S), text, font=f, fill=(0, 0, 0, 170))
        d.text((x, y * S), text, font=f, fill=col)
        return
    for ch, w in zip(text, widths):
        d.text((x, y * S + S), ch, font=f, fill=(0, 0, 0, 170))
        d.text((x, y * S), ch, font=f, fill=col)
        x += w + spacing * S


# ---------- header: chrome wordmark + full official TrapWin logo ----------
img = place_title(img, chrome_title("AURA VERB", 30 * S, glow=GOLD, glow_strength=1.25, tint=(14, 6, -18)), 24 * S, 14 * S)
_lg = trapwin_logo(36 * S)
img = place(img, _lg, 20 * S, 50 * S)
d = BlendDraw(globals(), "img")
d.text((20 * S + _lg.width + 12 * S, 62 * S), "MULTI-ALGORITHM STUDIO REVERB", font=font("BarlowCondensed-SemiBold.ttf", 13), fill=INK + (255,))

hl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
hd = ImageDraw.Draw(hl)
for x in range(16 * S, (BW - 16) * S):
    t = (x / S - 16) / (BW - 32)
    a = int(95 * math.sin(math.pi * t) ** 2)
    hd.point((x, 85 * S), fill=GOLD + (a,))
img = Image.alpha_composite(img, hl)
d = BlendDraw(globals(), "img")

# ---------- mode strip ----------
inset(MODE_STRIP, 9, fill=(7, 9, 18, 255), shadow=230)


# ---------- big dial seats ----------
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
    well[..., :3] = np.where(lip[..., None], (np.array([52, 56, 70]) + 100 * lit[..., None] ** 2), 0)
    shadowin = np.clip(1.0 - (R - 3 - rr2) / 16.0, 0, 1) * np.clip(0.5 - 0.5 * np.cos(ang - math.radians(45)), 0, 1)
    flo = np.array([9, 11, 20]) * (1.0 - 0.6 * shadowin[..., None])
    well[..., :3] = np.where(floor[..., None], flo, well[..., :3])
    well[..., 3] = np.where(lip | floor, 255, 0) * np.clip(R + 2 - rr2 + 0.5, 0, 1)
    img.alpha_composite(Image.fromarray(np.clip(well, 0, 255).astype(np.uint8), "RGBA"), (int((cx - R - 4) * S), int((cy - R - 4) * S)))
    d = BlendDraw(globals(), "img")
    d.arc(((cx - 61) * S, (cy - 61) * S, (cx + 61) * S, (cy + 61) * S), start=135, end=45, fill=(30, 32, 44, 255), width=5 * S)
    for i in range(n_ticks):
        a = math.radians(225 - 270 * i / (n_ticks - 1))
        major = i in majors
        r0, r1 = 74, (83 if major else 78)
        d.line(((cx + math.cos(a) * r0) * S, (cy - math.sin(a) * r0) * S, (cx + math.cos(a) * r1) * S, (cy - math.sin(a) * r1) * S),
               fill=(236, 226, 200, 220 if major else 90), width=S + (1 if major else 0))
    if names:
        for frac, t in names:
            a = math.radians(225 - 270 * frac)
            tx, ty = cx + math.cos(a) * 94, cy - math.sin(a) * 94
            tw = d.textlength(t, font=small)
            d.text((tx * S - tw / 2, (ty - 7) * S), t, font=small, fill=INK + (255,))


big_seat(*DECAY_C, names=[(0.0, "0.1s"), (1.0, "60s")])
big_seat(*MIX_C, n_ticks=21, majors=(0, 5, 10, 15, 20), names=[(0.0, "DRY"), (1.0, "WET")])
label("DECAY", DECAY_C[0], 126, lab_big, spacing=1.5)
label("MIX", MIX_C[0], 126, lab_big, spacing=1.5)


def lcd(box):
    """small gold-on-black LCD window (text painted live)"""
    global img, d
    inset(box, 6, fill=(6, 7, 12, 255), outline=(80, 86, 104, 170), shadow=230)
    x0, y0, x1, y1 = box
    g = np.zeros(((y1 - y0 - 4) * S, (x1 - x0 - 4) * S, 4), np.float32)
    g[..., :3] = (20, 16, 7)
    g[::3, :, :3] *= 0.7
    g[..., 3] = 255
    gi = Image.fromarray(np.clip(g, 0, 255).astype(np.uint8), "RGBA")
    m = Image.new("L", gi.size, 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, gi.size[0] - 1, gi.size[1] - 1), radius=4 * S, fill=255)
    img.paste(gi, ((x0 + 2) * S, (y0 + 2) * S), m)
    d = BlendDraw(globals(), "img")


lcd((DECAY_C[0] - LCD_W // 2, LCD_Y[0], DECAY_C[0] + LCD_W // 2, LCD_Y[1]))
lcd((MIX_C[0] - LCD_W // 2, LCD_Y[0], MIX_C[0] + LCD_W // 2, LCD_Y[1]))

# ---------- space screen ----------
inset(SCREEN, 12, fill=(5, 6, 12, 255), shadow=230)
x0, y0, x1, y1 = SCREEN
sw, sh_ = (x1 - x0 - 10) * S, (y1 - y0 - 10) * S
scr = np.zeros((sh_, sw, 3), np.float32)
syy, sxx = np.mgrid[0:sh_, 0:sw]
scr[:] = (8, 10, 20)
scr += (np.exp(-(((sxx - sw * 0.45) / (sw * 0.6)) ** 2 + ((syy - sh_ * 0.65) / (sh_ * 0.7)) ** 2)) * 12)[..., None] * np.array([0.9, 0.75, 0.4])
scr[::3] *= 0.84
scr += rng.normal(0, 0.9, scr.shape)
si = to_img(scr).convert("RGBA")
_SI = {"si": si}
sd = BlendDraw(_SI, "si")
gx0, gy0, gx1, gy1 = GRAPH
ox, oy = x0 + 5, y0 + 5
for k, dbv in enumerate((0, -20, -40, -60)):
    gy = (gy0 + (gy1 - gy0) * k / 3 - oy) * S
    sd.line(((gx0 - ox) * S, gy, (gx1 - ox) * S, gy), fill=GOLD + (34 if k in (0, 3) else 18,), width=1)
    sd.text(((gx0 - ox - 13) * S, gy - 5 * S), str(dbv), font=font("BarlowCondensed-SemiBold.ttf", 9), fill=INK + (150,))
m = Image.new("L", si.size, 0)
ImageDraw.Draw(m).rounded_rectangle((0, 0, si.size[0] - 1, si.size[1] - 1), radius=8 * S, fill=255)
si = _SI["si"]
img.paste(si, ((x0 + 5) * S, (y0 + 5) * S), m)
d = BlendDraw(globals(), "img")
d.line(((x0 + 14) * S, 150 * S, (x1 - 14) * S, 150 * S), fill=GOLD + (40,), width=S)
gl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
ImageDraw.Draw(gl).polygon([((x0 + 5) * S, (y0 + 5) * S), ((x0 + 150) * S, (y0 + 5) * S), ((x0 + 40) * S, (y1 - 5) * S), ((x0 + 5) * S, (y1 - 5) * S)],
                           fill=(255, 255, 255, 8))
img = Image.alpha_composite(img, gl)
d = BlendDraw(globals(), "img")

label("PRE-DELAY TIME", 310, 334, tiny, INK + (210,), spacing=0.8)
label("COLOUR", 546, 334, tiny, INK + (210,), spacing=0.8)

# ---------- bottom deck ----------
inset(DECK_L, 12, fill=(10, 13, 24, 255))
inset(DECK_R, 12, fill=(10, 13, 24, 255))


def bezel(box, r=10):
    global img, d
    x0, y0, x1, y1 = box
    rr((x0 - 4, y0 - 4, x1 + 4, y1 + 4), r + 3, fill=(5, 6, 11, 255))
    grad = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(grad)
    gd.rounded_rectangle(((x0 - 4) * S, (y0 - 4) * S, (x1 + 4) * S, (y1 + 4) * S), radius=(r + 3) * S, outline=(130, 136, 160, 120), width=S)
    gd.rounded_rectangle(((x0 - 2) * S, (y0 - 2) * S, (x1 + 2) * S, (y1 + 2) * S), radius=(r + 1) * S, outline=(0, 0, 0, 220), width=2 * S)
    img = Image.alpha_composite(img, grad)
    d = BlendDraw(globals(), "img")


bezel(FREEZE_BTN)
label("click = latch  ·  hold = momentary", 106, 438, tiny, INK + (190,))
# meter troughs (the editor paints the live bars)
for k, t in enumerate(("IN", "WET")):
    y = 462 + k * 20
    d.text((30 * S, (y - 6) * S), t, font=small, fill=INK + (255,))
    rr((58, y - 3, 184, y + 4), 3, fill=(4, 5, 10, 255), outline=(70, 76, 96, 160), width=S)
    for q in range(1, 6):
        xq = 58 + 126 * q / 6
        d.line((xq * S, (y + 6) * S, xq * S, (y + 8) * S), fill=INK + (120,), width=S)

for xs, ys, names in ((ROW1_X, ROW1_Y, ROW1_NAMES), (ROW2_X, ROW2_Y, ROW2_NAMES)):
    for cx, t in zip(xs, names):
        label(t, cx, ys - 40, lab)
        for k in range(21):
            a = math.radians(225 - 270 * k / 20)
            x, y = cx + math.cos(a) * 25.5, ys - math.sin(a) * 25.5
            d.ellipse(((x - 1.5) * S, (y - 1.5) * S, (x + 1.5) * S, (y + 1.5) * S), fill=(40, 44, 58, 255))
        d.ellipse(((cx - 21) * S, (ys - 21) * S, (cx + 21) * S, (ys + 21) * S), fill=(4, 5, 10, 255))

# group separators
for x, ya, yb in ((ROW1_X[4] + 45, 354, 434), (ROW2_X[2] + 45, 440, 522), (ROW2_X[4] + 45, 440, 522)):
    d.line((x * S, ya * S, x * S, yb * S), fill=(44, 48, 64, 255), width=S)
d.line(((DECK_R[0] + 14) * S, 435 * S, (DECK_R[2] - 14) * S, 435 * S), fill=(36, 40, 56, 255), width=S)

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

style = os.environ.get("AURA_KNOB", "black")
render_knob(320, style, seed=4).save(os.path.join(ROOT, "Resources", "knob_big.png"), optimize=True)
render_knob(128, style, seed=5).save(os.path.join(ROOT, "Resources", "knob_small.png"), optimize=True)
print("knobs:", style)
