"""Generates the TrapWin THROW editor background (Resources/bg.png) at 2x of the 820x500 base size,
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
BW, BH = 820, 500
W, H = BW * S, BH * S
ROOT = os.path.dirname(HERE)
FONTS = os.path.join(ROOT, "Resources", "fonts")
rng = np.random.default_rng(31)
AMBER = (255, 178, 46)
TEXT = (236, 230, 220)
INK = (146, 138, 126)

# ---- layout (base px) ----
TIME_C = (130, 196)
FB_C = (690, 196)
SCREEN = (252, 92, 568, 282)
LCD_W = 120
LCD_Y = (296, 326)
DECK_L = (16, 342, 216, 488)
DECK_R = (228, 342, 804, 488)
THROW_BTN = (28, 356, 148, 440)
FREEZE_BTN = (156, 356, 204, 440)
SMALL_Y = 410
SMALL_X = [264 + 72 * i for i in range(8)]
SMALL_NAMES = ["DRIVE", "WOBBLE", "LO CUT", "HI CUT", "DUCK", "WIDTH", "MIX", "OUTPUT"]


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
base += glow[..., None] * np.array([16, 10, 2])
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
img = place_title(img, chrome_title("THROW", 30 * S, glow=AMBER, glow_strength=0.8), 24 * S, 14 * S)
_lg = trapwin_logo(36 * S)
img = place(img, _lg, 20 * S, 50 * S)
d = BlendDraw(globals(), "img")
d.text((20 * S + _lg.width + 12 * S, 62 * S), "VOCAL THROW  ·  TAPE DELAY", font=font("BarlowCondensed-SemiBold.ttf", 13), fill=INK + (255,))

# thin amber hairline under the header
hl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
hd = ImageDraw.Draw(hl)
for x in range(16 * S, (BW - 16) * S):
    t = (x / S - 16) / (BW - 32)
    a = int(90 * math.sin(math.pi * t) ** 2)
    hd.point((x, 84 * S), fill=AMBER + (a,))
img = Image.alpha_composite(img, hl)
d = BlendDraw(globals(), "img")


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
    well[..., :3] = np.where(lip[..., None], (np.array([56, 52, 48]) + 100 * lit[..., None] ** 2), 0)
    shadowin = np.clip(1.0 - (R - 3 - rr2) / 16.0, 0, 1) * np.clip(0.5 - 0.5 * np.cos(ang - math.radians(45)), 0, 1)
    flo = np.array([13, 12, 12]) * (1.0 - 0.6 * shadowin[..., None])
    well[..., :3] = np.where(floor[..., None], flo, well[..., :3])
    well[..., 3] = np.where(lip | floor, 255, 0) * np.clip(R + 2 - rr2 + 0.5, 0, 1)
    img.alpha_composite(Image.fromarray(np.clip(well, 0, 255).astype(np.uint8), "RGBA"), (int((cx - R - 4) * S), int((cy - R - 4) * S)))
    d = BlendDraw(globals(), "img")
    # unlit arc groove (the editor paints the lit amber arc on top, radius 61)
    d.arc(((cx - 61) * S, (cy - 61) * S, (cx + 61) * S, (cy + 61) * S), start=135, end=45, fill=(36, 32, 28, 255), width=5 * S)
    for i in range(n_ticks):
        a = math.radians(225 - 270 * i / (n_ticks - 1))
        major = i in majors
        r0, r1 = 74, (83 if major else 78)
        d.line(((cx + math.cos(a) * r0) * S, (cy - math.sin(a) * r0) * S, (cx + math.cos(a) * r1) * S, (cy - math.sin(a) * r1) * S),
               fill=(232, 222, 205, 220 if major else 90), width=S + (1 if major else 0))
    if names:
        for frac, t in names:
            a = math.radians(225 - 270 * frac)
            tx, ty = cx + math.cos(a) * 94, cy - math.sin(a) * 94
            tw = d.textlength(t, font=small)
            d.text((tx * S - tw / 2, (ty - 7) * S), t, font=small, fill=INK + (255,))


big_seat(*TIME_C, names=[(0.0, "SHORT"), (1.0, "LONG")])
big_seat(*FB_C, n_ticks=23, majors=(0, 5, 10, 15, 20, 22), names=[(0.0, "0"), (100 / 110, "100"), (1.0, "INF")])
label("TIME", TIME_C[0], 92, lab_big, spacing=1.5)
label("FEEDBACK", FB_C[0], 92, lab_big, spacing=1.5)


def lcd(box):
    """small amber LCD window (text painted live)"""
    global img, d
    inset(box, 6, fill=(9, 8, 7, 255), outline=(70, 64, 56, 170), shadow=230)
    x0, y0, x1, y1 = box
    g = np.zeros(((y1 - y0 - 4) * S, (x1 - x0 - 4) * S, 4), np.float32)
    g[..., :3] = (22, 15, 6)
    g[::3, :, :3] *= 0.7
    g[..., 3] = 255
    gi = Image.fromarray(np.clip(g, 0, 255).astype(np.uint8), "RGBA")
    m = Image.new("L", gi.size, 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, gi.size[0] - 1, gi.size[1] - 1), radius=4 * S, fill=255)
    img.paste(gi, ((x0 + 2) * S, (y0 + 2) * S), m)
    d = BlendDraw(globals(), "img")


lcd((TIME_C[0] - LCD_W // 2, LCD_Y[0], TIME_C[0] + LCD_W // 2, LCD_Y[1]))
lcd((FB_C[0] - LCD_W // 2, LCD_Y[0], FB_C[0] + LCD_W // 2, LCD_Y[1]))

# ---------- tap screen ----------
inset(SCREEN, 12, fill=(8, 7, 7, 255), shadow=230)
x0, y0, x1, y1 = SCREEN
sw, sh_ = (x1 - x0 - 10) * S, (y1 - y0 - 10) * S
scr = np.zeros((sh_, sw, 3), np.float32)
syy, sxx = np.mgrid[0:sh_, 0:sw]
scr[:] = (14, 11, 8)
scr += (np.exp(-(((sxx - sw * 0.5) / (sw * 0.6)) ** 2 + ((syy - sh_ * 0.62) / (sh_ * 0.7)) ** 2)) * 10)[..., None] * np.array([1.0, 0.6, 0.15])
scr[::3] *= 0.82
scr += rng.normal(0, 1.0, scr.shape)
si = to_img(scr).convert("RGBA")
_SI = {"si": si}
sd = BlendDraw(_SI, "si")
# grid: baseline + time divisions (the editor draws the taps on top)
TAP_TOP, TAP_BASE = 118, 262            # base px, relative to the panel
gx0, gx1 = 14, (x1 - x0 - 10) - 10       # inside the screen image
for k in range(9):
    gx = (gx0 + (gx1 - gx0) * k / 8) * S
    sd.line((gx, (TAP_TOP - y0 - 5) * S, gx, (TAP_BASE - y0 - 5) * S), fill=(255, 178, 46, 16 if k % 2 else 30), width=1)
for k in range(5):
    gy = (TAP_TOP - y0 - 5 + (TAP_BASE - TAP_TOP) * k / 4) * S
    sd.line((gx0 * S, gy, gx1 * S, gy), fill=(255, 178, 46, 14), width=1)
sd.line((gx0 * S, (TAP_BASE - y0 - 5) * S, gx1 * S, (TAP_BASE - y0 - 5) * S), fill=(255, 178, 46, 60), width=S)
m = Image.new("L", si.size, 0)
ImageDraw.Draw(m).rounded_rectangle((0, 0, si.size[0] - 1, si.size[1] - 1), radius=8 * S, fill=255)
si = _SI["si"]
img.paste(si, ((x0 + 5) * S, (y0 + 5) * S), m)
# readout strip separator
d = BlendDraw(globals(), "img")
d.line(((x0 + 14) * S, 114 * S, (x1 - 14) * S, 114 * S), fill=(255, 178, 46, 40), width=S)
# glass reflection
gl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
ImageDraw.Draw(gl).polygon([((x0 + 5) * S, (y0 + 5) * S), ((x0 + 150) * S, (y0 + 5) * S), ((x0 + 40) * S, (y1 - 5) * S), ((x0 + 5) * S, (y1 - 5) * S)],
                           fill=(255, 255, 255, 9))
img = Image.alpha_composite(img, gl)
d = BlendDraw(globals(), "img")

# selector captions under the screen are drawn by the buttons themselves; small engraved captions:
label("SYNC", 310, 324, tiny, INK + (200,), spacing=1.0)
label("MODE", 474, 324, tiny, INK + (200,), spacing=1.0)

# ---------- bottom deck ----------
inset(DECK_L, 12, fill=(16, 15, 15, 255))
inset(DECK_R, 12, fill=(16, 15, 15, 255))


def bezel(box, r=10):
    """machined bezel the illuminated button sits in"""
    global img, d
    x0, y0, x1, y1 = box
    rr((x0 - 4, y0 - 4, x1 + 4, y1 + 4), r + 3, fill=(8, 8, 9, 255))
    grad = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(grad)
    gd.rounded_rectangle(((x0 - 4) * S, (y0 - 4) * S, (x1 + 4) * S, (y1 + 4) * S), radius=(r + 3) * S, outline=(120, 112, 100, 120), width=S)
    gd.rounded_rectangle(((x0 - 2) * S, (y0 - 2) * S, (x1 + 2) * S, (y1 + 2) * S), radius=(r + 1) * S, outline=(0, 0, 0, 220), width=2 * S)
    img = Image.alpha_composite(img, grad)
    d = BlendDraw(globals(), "img")


bezel(THROW_BTN)
bezel(FREEZE_BTN)
label("click = latch  ·  hold = momentary", 116, 470, tiny, INK + (190,))

for cx, t in zip(SMALL_X, SMALL_NAMES):
    label(t, cx, 349, lab)
    cy = SMALL_Y
    for k in range(21):
        a = math.radians(225 - 270 * k / 20)
        x, y = cx + math.cos(a) * 29, cy - math.sin(a) * 29
        d.ellipse(((x - 1.6) * S, (y - 1.6) * S, (x + 1.6) * S, (y + 1.6) * S), fill=(46, 42, 38, 255))
    d.ellipse(((cx - 24) * S, (cy - 24) * S, (cx + 24) * S, (cy + 24) * S), fill=(7, 7, 8, 255))

# group captions + separators
groups = [("TAPE", 0, 1), ("ECHO FILTER", 2, 3), ("SPACE", 4, 5), ("OUTPUT", 6, 7)]
for name, a, b in groups:
    xa, xb = SMALL_X[a] - 26, SMALL_X[b] + 26
    cx = (xa + xb) / 2
    tw = d.textlength(name, font=tiny) / S
    d.line((xa * S, 478 * S, (cx - tw / 2 - 6) * S, 478 * S), fill=(255, 178, 46, 70), width=S)
    d.line(((cx + tw / 2 + 6) * S, 478 * S, xb * S, 478 * S), fill=(255, 178, 46, 70), width=S)
    label(name, cx, 471, tiny, AMBER + (190,))
for i in (1, 3, 5):
    x = (SMALL_X[i] + SMALL_X[i + 1]) / 2
    d.line((x * S, 354 * S, x * S, 466 * S), fill=(50, 46, 42, 255), width=S)

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

# ---------- knobs (static image; the editor draws the rotating pointer) ----------
style = os.environ.get("THROW_KNOB", "black")
render_knob(320, style, seed=4).save(os.path.join(ROOT, "Resources", "knob_big.png"), optimize=True)
render_knob(128, style, seed=5).save(os.path.join(ROOT, "Resources", "knob_small.png"), optimize=True)
print("knobs:", style)
