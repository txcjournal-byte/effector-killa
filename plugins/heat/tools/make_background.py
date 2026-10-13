"""Generates the TrapWin HEAT editor background (Resources/bg.png) at 2x of the 860x540 base size,
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
rng = np.random.default_rng(57)
CORAL = (255, 90, 54)
EMBER = (255, 140, 60)
TEXT = (238, 228, 222)
INK = (150, 132, 126)

# ---- layout (base px) ----
DISPLAY = (16, 92, 574, 302)
CURVE = (26, 116, 204, 292)
SPEC = (214, 116, 564, 292)
GTOP = (586, 92, 844, 302)
GBOT = (586, 312, 844, 532)
BAND_X = [16, 205, 394]
BAND_W = 180
BAND_Y = (312, 532)
MODEL_LCD_DY = (346, 372)
DRIVE_DX, DRIVE_Y, DRIVE_R = 64, 442, 48
SMALL_DX, MIX_Y, LEVEL_Y, SMALL_R = 146, 414, 486, 22
TOP_KNOBS_X = [629, 715, 801]
TOP_KNOB_Y, TOP_KNOB_R = 222, 28
TONE_X = [618, 682, 746, 810]
TONE_Y, TONE_R = 362, 24
OUT_X = [737, 809]
OUT_Y, OUT_R = 472, 26
AUTO_BTN = (598, 436, 676, 506)


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


# ---------- chassis: volcanic charcoal, fine grain, ember glow rising from below ----------
base = np.zeros((H, W, 3), np.float32)
base[:] = (21, 17, 17)
yy, xx = np.mgrid[0:H, 0:W]
grain = rng.normal(0, 1, (H, W))
grain = np.asarray(Image.fromarray(np.uint8(np.clip(128 + grain * 30, 0, 255))).filter(ImageFilter.GaussianBlur(0.8)), np.float32) - 128
mottle = rng.normal(0, 1, (H // 16, W // 16))
mottle = np.asarray(Image.fromarray(np.uint8(np.clip(128 + mottle * 40, 0, 255))).resize((W, H), Image.BICUBIC).filter(ImageFilter.GaussianBlur(10)), np.float32) - 128
base += grain[..., None] * 0.10 + mottle[..., None] * 0.06
# ember glow from the bottom edge + warm key light top-left
glow_b = np.exp(-(((xx - W * 0.5) / (W * 0.62)) ** 2 + ((yy - H * 1.08) / (H * 0.42)) ** 2))
base += glow_b[..., None] * np.array([70, 18, 6])
glow_t = np.exp(-(((xx - W * 0.22) / (W * 0.45)) ** 2 + ((yy - H * 0.05) / (H * 0.35)) ** 2))
base += glow_t[..., None] * np.array([14, 8, 6])
vig = 1.0 - 0.42 * (((xx - W / 2) / (W / 2)) ** 2 + ((yy - H / 2) / (H / 2)) ** 2) ** 1.5
base *= vig[..., None]
img = to_img(base).convert("RGBA")

# drifting embers (sparks) - mostly low and at the edges; panels cover the centre later
sp = Image.new("RGBA", (W, H), (0, 0, 0, 0))
spd = ImageDraw.Draw(sp)
for _ in range(170):
    x = rng.uniform(0, W)
    y = H * (1.0 - rng.beta(1.3, 3.2))
    r = rng.uniform(0.5, 1.8) * S
    hot = rng.uniform(0, 1)
    col = (255, int(90 + 120 * hot), int(40 + 60 * hot))
    a = int(rng.uniform(70, 230))
    spd.ellipse((x - r, y - r, x + r, y + r), fill=col + (a,))
blur = sp.filter(ImageFilter.GaussianBlur(4 * S))
img = Image.alpha_composite(img, blur)
img = Image.alpha_composite(img, blur)
img = Image.alpha_composite(img, sp)
d = BlendDraw(globals(), "img")


def rr(box, r, **kw):
    x0, y0, x1, y1 = box
    d.rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=r * S, **kw)


def inset(box, r=12, fill=(15, 12, 12, 255), outline=(96, 72, 64, 150), shadow=210):
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
    d.line(((x0 + r) * S, y1 * S + S, (x1 - r) * S, y1 * S + S), fill=(150, 110, 96, 70), width=S)


def panel_floor(box, r=12):
    """faint warm floor gradient inside a panel (heat rising from below)"""
    global img, d
    x0, y0, x1, y1 = box
    hgt = (y1 - y0) * S
    g = np.zeros((hgt, (x1 - x0) * S, 4), np.float32)
    t = np.linspace(0, 1, hgt)[:, None]
    g[..., 0] = 255
    g[..., 1] = 70
    g[..., 2] = 40
    g[..., 3] = (t ** 3 * 16)
    gi = Image.fromarray(np.clip(g, 0, 255).astype(np.uint8), "RGBA")
    m = Image.new("L", gi.size, 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, gi.size[0] - 1, gi.size[1] - 1), radius=r * S, fill=255)
    gi.putalpha(Image.fromarray(np.minimum(np.asarray(gi.getchannel("A")), np.asarray(m))))
    img.alpha_composite(gi, (x0 * S, y0 * S))
    d = BlendDraw(globals(), "img")


lab = font("BarlowCondensed-Bold.ttf", 14)
lab_big = font("BarlowCondensed-Bold.ttf", 16)
small = font("BarlowCondensed-Bold.ttf", 12)
tiny = font("BarlowCondensed-SemiBold.ttf", 11)


def label(text, cx, y, f=lab, col=TEXT + (255,), spacing=0.0, anchor="c"):
    widths = [d.textlength(ch, font=f) for ch in text]
    tw = sum(widths) + spacing * S * (len(text) - 1)
    x = cx * S - tw / 2 if anchor == "c" else (cx * S if anchor == "l" else cx * S - tw)
    for ch, w in zip(text, widths):
        d.text((x, y * S + S), ch, font=f, fill=(0, 0, 0, 170))
        d.text((x, y * S), ch, font=f, fill=col)
        x += w + spacing * S


# ---------- header: chrome wordmark + full official TrapWin logo ----------
img = place_title(img, chrome_title("HEAT", 30 * S, glow=CORAL, glow_strength=1.0), 24 * S, 14 * S)
_lg = trapwin_logo(36 * S)
img = place(img, _lg, 20 * S, 50 * S)
d = BlendDraw(globals(), "img")
d.text((20 * S + _lg.width + 12 * S, 62 * S), "MULTIBAND SATURATION  ·  DISTORTION", font=font("BarlowCondensed-SemiBold.ttf", 13), fill=INK + (255,))

hl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
hd = ImageDraw.Draw(hl)
for x in range(16 * S, (BW - 16) * S):
    t = (x / S - 16) / (BW - 32)
    a = int(110 * math.sin(math.pi * t) ** 2)
    hd.point((x, 84 * S), fill=CORAL + (a,))
img = Image.alpha_composite(img, hl.filter(ImageFilter.GaussianBlur(S)))
img = Image.alpha_composite(img, hl)
d = BlendDraw(globals(), "img")


def screen(box, r=8):
    """dark glass screen with scanlines (contents painted live)"""
    global img, d
    inset(box, r, fill=(8, 6, 6, 255), outline=(80, 60, 54, 170), shadow=235)
    x0, y0, x1, y1 = box
    sw, sh_ = (x1 - x0 - 4) * S, (y1 - y0 - 4) * S
    scr = np.zeros((sh_, sw, 3), np.float32)
    syy, sxx = np.mgrid[0:sh_, 0:sw]
    scr[:] = (13, 9, 9)
    scr += (np.exp(-(((sxx - sw * 0.5) / (sw * 0.7)) ** 2 + ((syy - sh_ * 0.9) / (sh_ * 0.7)) ** 2)) * 12)[..., None] * np.array([1.0, 0.35, 0.2])
    scr[::3] *= 0.84
    scr += rng.normal(0, 0.9, scr.shape)
    si = to_img(scr).convert("RGBA")
    m = Image.new("L", si.size, 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, si.size[0] - 1, si.size[1] - 1), radius=(r - 2) * S, fill=255)
    img.paste(si, ((x0 + 2) * S, (y0 + 2) * S), m)
    d = BlendDraw(globals(), "img")


def lcd(box, r=5):
    """small LCD window (text painted live)"""
    global img, d
    inset(box, r, fill=(9, 7, 7, 255), outline=(76, 58, 52, 170), shadow=230)
    x0, y0, x1, y1 = box
    g = np.zeros(((y1 - y0 - 4) * S, (x1 - x0 - 4) * S, 4), np.float32)
    g[..., :3] = (24, 9, 7)
    g[::3, :, :3] *= 0.7
    g[..., 3] = 255
    gi = Image.fromarray(np.clip(g, 0, 255).astype(np.uint8), "RGBA")
    m = Image.new("L", gi.size, 0)
    ImageDraw.Draw(m).rounded_rectangle((0, 0, gi.size[0] - 1, gi.size[1] - 1), radius=max(1, r - 2) * S, fill=255)
    img.paste(gi, ((x0 + 2) * S, (y0 + 2) * S), m)
    d = BlendDraw(globals(), "img")


def seat(cx, cy, R, ticks=True):
    """knob well with an unlit arc groove (the editor paints the lit arc) and scale ticks"""
    global img, d
    pad = 6
    sz = int((2 * R + 2 * pad) * S)
    yy2, xx2 = np.mgrid[0:sz, 0:sz].astype(np.float32)
    cc = sz / 2
    rr2 = np.sqrt((xx2 - cc) ** 2 + (yy2 - cc) ** 2) / S
    ang = np.arctan2(yy2 - cc, xx2 - cc)
    lip = (rr2 > R - 2) & (rr2 <= R + 1.5)
    floor = rr2 <= R - 2
    well = np.zeros(rr2.shape + (4,), np.float32)
    lit = 0.5 + 0.5 * np.cos(ang - math.radians(45))
    well[..., :3] = np.where(lip[..., None], (np.array([52, 44, 42]) + 90 * lit[..., None] ** 2), 0)
    flo = np.array([11, 9, 9]) * np.ones_like(rr2)[..., None]
    well[..., :3] = np.where(floor[..., None], flo, well[..., :3])
    well[..., 3] = np.where(lip | floor, 255, 0) * np.clip(R + 1.5 - rr2 + 0.5, 0, 1)
    img.alpha_composite(Image.fromarray(np.clip(well, 0, 255).astype(np.uint8), "RGBA"), (int((cx - R - pad) * S), int((cy - R - pad) * S)))
    d = BlendDraw(globals(), "img")
    ar = R - 5
    w = 4 if R > 30 else 3
    d.arc(((cx - ar) * S, (cy - ar) * S, (cx + ar) * S, (cy + ar) * S), start=135, end=45, fill=(40, 30, 28, 255), width=w * S)
    if ticks:
        n = 11
        for i in range(n):
            a = math.radians(225 - 270 * i / (n - 1))
            r0, r1 = R + 4, R + (8 if i in (0, n // 2, n - 1) else 6)
            d.line(((cx + math.cos(a) * r0) * S, (cy - math.sin(a) * r0) * S, (cx + math.cos(a) * r1) * S, (cy - math.sin(a) * r1) * S),
                   fill=(220, 200, 190, 170 if i in (0, n // 2, n - 1) else 70), width=S)


# ---------- display: transfer curve + spectrum ----------
inset(DISPLAY, 12, fill=(13, 10, 10, 255))
screen(CURVE, 8)
screen(SPEC, 8)
# transfer curve grid (editor draws the curve)
cx0, cy0, cx1, cy1 = CURVE[0] + 8, CURVE[1] + 8, CURVE[2] - 8, CURVE[3] - 8
for k in range(9):
    gx = cx0 + (cx1 - cx0) * k / 8
    gy = cy0 + (cy1 - cy0) * k / 8
    a = 46 if k == 4 else 16
    d.line((gx * S, cy0 * S, gx * S, cy1 * S), fill=CORAL + (a,), width=1)
    d.line((cx0 * S, gy * S, cx1 * S, gy * S), fill=CORAL + (a,), width=1)
d.line((cx0 * S, cy1 * S, cx1 * S, cy0 * S), fill=(255, 255, 255, 18), width=1)
# spectrum grid: log frequency + dB lines (editor maps 20 Hz..20 kHz, 0..-84 dB)
sx0, sy0, sx1, sy1 = SPEC[0] + 8, SPEC[1] + 8, SPEC[2] - 8, SPEC[3] - 20


def fx(f):
    return sx0 + (sx1 - sx0) * math.log(f / 20.0) / math.log(1000.0)


for f in (20, 30, 40, 50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 20000):
    major = f in (100, 1000, 10000)
    d.line((fx(f) * S, sy0 * S, fx(f) * S, sy1 * S), fill=CORAL + (40 if major else 13,), width=1)
for k in range(1, 7):
    gy = sy0 + (sy1 - sy0) * k / 7
    d.line((sx0 * S, gy * S, sx1 * S, gy * S), fill=CORAL + (14,), width=1)
for f, t in ((50, "50"), (100, "100"), (200, "200"), (500, "500"), (1000, "1k"), (2000, "2k"), (5000, "5k"), (10000, "10k")):
    tw = d.textlength(t, font=tiny) / S
    d.text(((fx(f) - tw / 2) * S, (sy1 + 3) * S), t, font=tiny, fill=INK + (190,))

# ---------- global top: bands / quality / punch / dynamics / low keep ----------
inset(GTOP, 12)
panel_floor(GTOP)
label("BANDS", 598, 108, small, INK + (230,), spacing=1.0, anchor="l")
label("QUALITY", 598, 142, small, INK + (230,), spacing=1.0, anchor="l")
for x, t in zip(TOP_KNOBS_X, ("PUNCH", "DYNAMICS", "LOW KEEP")):
    label(t, x, 176, lab, spacing=0.8)
    seat(x, TOP_KNOB_Y, TOP_KNOB_R)
    lcd((x - 36, 262, x + 36, 282))

# ---------- band modules ----------
for i, x0 in enumerate(BAND_X):
    box = (x0, BAND_Y[0], x0 + BAND_W, BAND_Y[1])
    inset(box, 12)
    panel_floor(box)
    lcd((x0 + 10, MODEL_LCD_DY[0], x0 + BAND_W - 10, MODEL_LCD_DY[1]))
    label("DRIVE", x0 + DRIVE_DX, 380, lab, spacing=0.8)
    seat(x0 + DRIVE_DX, DRIVE_Y, DRIVE_R)
    lcd((x0 + DRIVE_DX - 34, 504, x0 + DRIVE_DX + 34, 524))
    label("MIX", x0 + SMALL_DX, 380, small, spacing=0.6)
    seat(x0 + SMALL_DX, MIX_Y, SMALL_R, ticks=False)
    label("LEVEL", x0 + SMALL_DX, 452, small, spacing=0.6)
    seat(x0 + SMALL_DX, LEVEL_Y, SMALL_R, ticks=False)

# ---------- global bottom: tone + output ----------
inset(GBOT, 12)
panel_floor(GBOT)
for x, t in zip(TONE_X, ("TILT PRE", "TILT POST", "LOW CUT", "HIGH CUT")):
    label(t, x, 318, small, spacing=0.4)
    seat(x, TONE_Y, TONE_R, ticks=False)
d.line((598 * S, 416 * S, 832 * S, 416 * S), fill=(60, 44, 40, 255), width=S)
for x, t in zip(OUT_X, ("MIX", "OUTPUT")):
    label(t, x, 420, small, spacing=0.8)
    seat(x, OUT_Y, OUT_R)


def bezel(box, r=10):
    global img, d
    x0, y0, x1, y1 = box
    rr((x0 - 4, y0 - 4, x1 + 4, y1 + 4), r + 3, fill=(8, 7, 7, 255))
    grad = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(grad)
    gd.rounded_rectangle(((x0 - 4) * S, (y0 - 4) * S, (x1 + 4) * S, (y1 + 4) * S), radius=(r + 3) * S, outline=(130, 104, 96, 120), width=S)
    gd.rounded_rectangle(((x0 - 2) * S, (y0 - 2) * S, (x1 + 2) * S, (y1 + 2) * S), radius=(r + 1) * S, outline=(0, 0, 0, 220), width=2 * S)
    img = Image.alpha_composite(img, grad)
    d = BlendDraw(globals(), "img")


bezel(AUTO_BTN, 9)

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
style = os.environ.get("HEAT_KNOB", "black")
render_knob(320, style, seed=4).save(os.path.join(ROOT, "Resources", "knob_big.png"), optimize=True)
render_knob(160, style, seed=5).save(os.path.join(ROOT, "Resources", "knob_small.png"), optimize=True)
print("knobs:", style)
