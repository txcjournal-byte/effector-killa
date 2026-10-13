"""Generates the PUMP editor background (Resources/bg.png) at 2x of the 820x500 base size.
Run:  python3 tools/make_background.py
"""
import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

S = 2
BW, BH = 820, 500
W, H = BW * S, BH * S
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONTS = os.path.join(ROOT, "Resources", "fonts")
rng = np.random.default_rng(11)

LIME = (182, 255, 59)


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), int(size * S))


def to_img(a):
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))


def knob_centre(i):
    kw = (BW - 32) / 6.0
    return 16 + i * kw + kw / 2, 430


# ---------- chassis: anodised gunmetal, horizontal brushing ----------
base = np.zeros((H, W, 3), np.float32)
base[:] = (22, 24, 21)
brush = rng.normal(0, 1, (H, 1)) * 3.0 + rng.normal(0, 1, (H, W)) * 1.6
brush = np.asarray(to_img(128 + brush * 5).filter(ImageFilter.GaussianBlur((6, 0)) if False else ImageFilter.BoxBlur(0)), np.float32) - 128
base += brush[..., None] * 0.55
yy, xx = np.mgrid[0:H, 0:W]
vig = 1.0 - 0.30 * (((xx - W / 2) / (W / 2)) ** 2 + ((yy - H / 2) / (H / 2)) ** 2) ** 1.5
base *= vig[..., None]
img = to_img(base).convert("RGBA")
d = ImageDraw.Draw(img)

# top bevel highlight
d.line((10 * S, 2 * S, W - 10 * S, 2 * S), fill=(90, 96, 86, 120), width=S)

# ---------- logo with lime glow ----------
glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
gd = ImageDraw.Draw(glow)
logo_font = font("ArchivoBlack-Regular.ttf", 46)
gd.text((24 * S, 10 * S), "PUMP", font=logo_font, fill=LIME + (200,))
glow = glow.filter(ImageFilter.GaussianBlur(9 * S))
img = Image.alpha_composite(img, glow)
d = ImageDraw.Draw(img)
d.text((24 * S, 10 * S), "PUMP", font=logo_font, fill=LIME + (255,))
d.text((28 * S, 62 * S), "TRAPWIN  /  VOLUME SHAPER & SIDECHAIN", font=font("BarlowCondensed-SemiBold.ttf", 13),
       fill=(125, 134, 118, 255))

# dropdown labels (engraved)
lab = font("BarlowCondensed-Bold.ttf", 11.5)
for x, t in ((300, "SHAPE"), (472, "RATE"), (594, "TRIGGER"), (716, "BAND")):
    d.text((x * S, 11 * S), t, font=lab, fill=(125, 134, 118, 255))

# ---------- screen bezel + glass ----------
sx0, sy0, sx1, sy1 = 16, 82, BW - 16, 362
d.rounded_rectangle(((sx0 - 4) * S, (sy0 - 4) * S, (sx1 + 4) * S, (sy1 + 4) * S), radius=14 * S, fill=(8, 9, 8, 255))
d.rounded_rectangle(((sx0 - 4) * S, (sy0 - 4) * S, (sx1 + 4) * S, (sy1 + 4) * S), radius=14 * S, outline=(70, 76, 66, 255), width=S)
screen = np.zeros(((sy1 - sy0) * S, (sx1 - sx0) * S, 3), np.float32)
screen[:] = (11, 15, 9)
# scanlines
screen[::3, :, :] *= 0.80
sh, sw = screen.shape[:2]
syy, sxx = np.mgrid[0:sh, 0:sw]
screen *= (1.0 - 0.25 * (((sxx - sw / 2) / (sw / 2)) ** 2 + ((syy - sh / 2) / (sh / 2)) ** 2))[..., None]
screen += rng.normal(0, 1.4, screen.shape)
scr = to_img(screen).convert("RGBA")
m = Image.new("L", scr.size, 0)
ImageDraw.Draw(m).rounded_rectangle((0, 0, scr.size[0] - 1, scr.size[1] - 1), radius=10 * S, fill=255)
img.paste(scr, (sx0 * S, sy0 * S), m)
d = ImageDraw.Draw(img)

# ---------- control deck ----------
dx0, dy0, dx1, dy1 = 16, 372, BW - 16, 488
d.rounded_rectangle((dx0 * S, dy0 * S, dx1 * S, dy1 * S), radius=10 * S, fill=(17, 19, 16, 255))
inner = Image.new("RGBA", (W, H), (0, 0, 0, 0))
ImageDraw.Draw(inner).rounded_rectangle((dx0 * S, dy0 * S, dx1 * S, dy1 * S), radius=10 * S, outline=(0, 0, 0, 200), width=4 * S)
inner = inner.filter(ImageFilter.GaussianBlur(4 * S))
img = Image.alpha_composite(img, inner)
d = ImageDraw.Draw(img)
d.rounded_rectangle((dx0 * S, dy0 * S, dx1 * S, dy1 * S), radius=10 * S, outline=(60, 66, 56, 140), width=S)

names = ["DEPTH", "SMOOTH", "X-OVER", "THRESH", "MIX", "OUTPUT"]
for i, n in enumerate(names):
    cx, cy = knob_centre(i)
    # unlit LED dots ring
    for k in range(21):
        a = math.radians(225 - 270 * k / 20)
        r = 38
        x, y = cx + math.cos(a) * r, cy - math.sin(a) * r
        d.ellipse(((x - 1.8) * S, (y - 1.8) * S, (x + 1.8) * S, (y + 1.8) * S), fill=(40, 46, 36, 255))
    # rubber knob well
    d.ellipse(((cx - 31) * S, (cy - 31) * S, (cx + 31) * S, (cy + 31) * S), fill=(6, 7, 6, 255))
    # separator lines between knobs
    if i > 0:
        x = cx - (BW - 32) / 12.0
        d.line((x * S, (dy0 + 14) * S, x * S, (dy1 - 14) * S), fill=(36, 40, 33, 255), width=S)

# screws
for (x, y) in ((8, 8), (BW - 8, 8), (8, BH - 8), (BW - 8, BH - 8)):
    d.ellipse(((x - 4.5) * S, (y - 4.5) * S, (x + 4.5) * S, (y + 4.5) * S), fill=(70, 74, 66, 255), outline=(10, 10, 10, 255), width=S)
    d.line(((x - 3) * S, (y - 1) * S, (x + 3) * S, (y + 1) * S), fill=(20, 22, 19, 255), width=S + 1)

out = os.path.join(ROOT, "Resources", "bg.png")
img.convert("RGB").save(out, optimize=True)
print("wrote", out, img.size)
