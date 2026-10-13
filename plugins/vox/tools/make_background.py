"""Generates the VOX-1 editor background (Resources/bg.png) at 2x of the 820x500 base size.
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
rng = np.random.default_rng(23)
MAG = (255, 63, 164)
INK = (150, 134, 170)


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), int(size * S))


def to_img(a):
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))


# ---------- chassis: dark violet metal with diagonal brushing ----------
base = np.zeros((H, W, 3), np.float32)
base[:] = (22, 16, 30)
yy, xx = np.mgrid[0:H, 0:W]
brush = rng.normal(0, 1, (H, 1)) * 2.5 + rng.normal(0, 1, (H, W)) * 1.4
brush = np.asarray(Image.fromarray(np.uint8(np.clip(128 + brush * 6, 0, 255))).filter(ImageFilter.BoxBlur(1)), np.float32) - 128
base += brush[..., None] * 0.45
light = 1.0 + 0.16 * (1.0 - (xx / W) * 0.5 - (yy / H))
base *= light[..., None]
glow = np.exp(-(((xx - W * 0.5) / (W * 0.6)) ** 2 + ((yy - H * 0.35) / (H * 0.5)) ** 2))
base += glow[..., None] * np.array([18, 6, 22])
vig = 1.0 - 0.35 * (((xx - W / 2) / (W / 2)) ** 2 + ((yy - H / 2) / (H / 2)) ** 2) ** 1.5
base *= vig[..., None]
img = to_img(base).convert("RGBA")
d = ImageDraw.Draw(img)


def rr(box, r, **kw):
    x0, y0, x1, y1 = box
    d.rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=r * S, **kw)


def inset(box, r=12, fill=(14, 10, 20, 255)):
    global img, d
    rr(box, r, fill=fill)
    sh = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    x0, y0, x1, y1 = box
    ImageDraw.Draw(sh).rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=r * S, outline=(0, 0, 0, 200), width=5 * S)
    sh = sh.filter(ImageFilter.GaussianBlur(5 * S))
    m = Image.new("L", (W, H), 0)
    ImageDraw.Draw(m).rounded_rectangle((x0 * S, y0 * S, x1 * S, y1 * S), radius=r * S, fill=255)
    sh.putalpha(Image.fromarray(np.minimum(np.asarray(sh.getchannel("A")), np.asarray(m))))
    img = Image.alpha_composite(img, sh)
    d = ImageDraw.Draw(img)
    rr(box, r, outline=(90, 70, 110, 140), width=S)


# ---------- logo: TrapWin chrome title + official TrapWin logo ----------
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from brand import chrome_title, place, place_title, trapwin_logo
img = place_title(img, chrome_title("VOX DEMON", 30 * S, glow=MAG, glow_strength=0.75), 24 * S, 14 * S)
_lg = trapwin_logo(36 * S)
img = place(img, _lg, 20 * S, 50 * S)
d = ImageDraw.Draw(img)
d.text((20 * S + _lg.width + 10 * S, 62 * S), "PITCH  ·  FORMANT  ·  HARD TUNE", font=font("BarlowCondensed-SemiBold.ttf", 13), fill=INK + (255,))

lab = font("BarlowCondensed-Bold.ttf", 15)
small = font("BarlowCondensed-Bold.ttf", 12)


def label(text, cx, y, f=lab, col=(230, 220, 240, 255)):
    tw = d.textlength(text, font=f)
    d.text((cx * S - tw / 2, y * S + S), text, font=f, fill=(0, 0, 0, 170))      # engraved shadow
    d.text((cx * S - tw / 2, y * S), text, font=f, fill=col)


# ---------- big dials ----------
def big_seat(cx, cy, r, lo, hi, marks):
    global img, d
    R = r + 16
    # machined well: dark inner floor, lit lower-right lip, shadowed upper-left lip, inner shadow
    yy2, xx2 = np.mgrid[0:(2 * R + 8) * S, 0:(2 * R + 8) * S].astype(np.float32)
    cc = (R + 4) * S
    rr2 = np.sqrt((xx2 - cc) ** 2 + (yy2 - cc) ** 2) / S
    ang = np.arctan2(yy2 - cc, xx2 - cc)
    lip = (rr2 > R - 3) & (rr2 <= R + 2)
    floor = rr2 <= R - 3
    well = np.zeros(rr2.shape + (4,), np.float32)
    lit = 0.5 + 0.5 * np.cos(ang - math.radians(45))          # lower-right lip catches the light
    well[..., :3] = np.where(lip[..., None], (np.array([60, 48, 74]) + 110 * lit[..., None] ** 2), 0)
    shadowin = np.clip(1.0 - (R - 3 - rr2) / 14.0, 0, 1) * np.clip(0.5 - 0.5 * np.cos(ang - math.radians(45)), 0, 1)
    flo = np.array([16, 11, 22]) * (1.0 - 0.6 * shadowin[..., None])
    well[..., :3] = np.where(floor[..., None], flo, well[..., :3])
    well[..., 3] = np.where(lip | floor, 255, 0) * np.clip(R + 2 - rr2 + 0.5, 0, 1)
    wimg = Image.fromarray(np.clip(well, 0, 255).astype(np.uint8), "RGBA")
    img.alpha_composite(wimg, (int((cx - R - 4) * S), int((cy - R - 4) * S)))
    d = ImageDraw.Draw(img)
    # LED arc groove
    d.arc(((cx - r - 8) * S, (cy - r - 8) * S, (cx + r + 8) * S, (cy + r + 8) * S), start=135, end=45, fill=(34, 24, 44, 255), width=7 * S)
    steps = int(hi - lo)
    for i in range(steps + 1):
        v = lo + i
        a = math.radians(225 - 270 * i / steps)
        major = v in marks
        r0, r1 = r + 20, r + (30 if major else 25)
        d.line(((cx + math.cos(a) * r0) * S, (cy - math.sin(a) * r0) * S, (cx + math.cos(a) * r1) * S, (cy - math.sin(a) * r1) * S),
               fill=(230, 210, 240, 230 if major else 110), width=S + (1 if major else 0))
        if major:
            t = ("+" if v > 0 else "") + str(v)
            tx, ty = cx + math.cos(a) * (r + 42), cy - math.sin(a) * (r + 42)
            tw = d.textlength(t, font=small)
            d.text((tx * S - tw / 2, (ty - 7) * S), t, font=small, fill=INK + (255,))


big_seat(150, 232, 70, -24, 24, (-24, -12, 0, 12, 24))
big_seat(670, 232, 70, -12, 12, (-12, -6, 0, 6, 12))
label("PITCH", 150, 86)
label("FORMANT", 670, 86)

# ---------- note display ----------
inset((300, 108, 520, 330), 14, fill=(10, 6, 14, 255))
scr = np.zeros(((330 - 108 - 12) * S, (520 - 300 - 12) * S, 3), np.float32)
scr[:] = (16, 8, 20)
scr[::3] *= 0.8
scr += rng.normal(0, 1.2, scr.shape)
si = to_img(scr).convert("RGBA")
m = Image.new("L", si.size, 0)
ImageDraw.Draw(m).rounded_rectangle((0, 0, si.size[0] - 1, si.size[1] - 1), radius=9 * S, fill=255)
img.paste(si, (306 * S, 114 * S), m)
gl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
ImageDraw.Draw(gl).polygon([(306 * S, 114 * S), (420 * S, 114 * S), (330 * S, 324 * S), (306 * S, 324 * S)], fill=(255, 255, 255, 14))
img = Image.alpha_composite(img, gl)
d = ImageDraw.Draw(img)
label("INPUT", 360, 122, small, INK + (255,))
label("TARGET", 460, 122, small, INK + (255,))
# cents scale
for c in range(-50, 51, 10):
    x = 410 + c * 1.7
    hh = 9 if c % 50 == 0 else (6 if c == 0 else 4)
    d.line((x * S, (268 - hh) * S, x * S, 268 * S), fill=(200, 170, 210, 200 if c in (-50, 0, 50) else 100), width=S)
label("-50", 325, 274, small, INK + (200,))
label("0", 410, 274, small, INK + (200,))
label("+50", 495, 274, small, INK + (200,))
label("CENTS", 410, 296, small, INK + (160,))

# ---------- bottom deck ----------
inset((16, 346, BW - 16, 488), 12, fill=(17, 12, 24, 255))
label("TUNE", 162, 352, small, INK + (255,))
label("KEY", 162, 402, small, INK + (255,))
label("SCALE", 386, 352, small, INK + (255,))
for cx, t in ((560, "RETUNE"), (660, "MIX"), (760, "OUTPUT")):
    label(t, cx, 352)
    # knob seats + LED dots
    cy = 418
    for k in range(21):
        a = math.radians(225 - 270 * k / 20)
        x, y = cx + math.cos(a) * 38, cy - math.sin(a) * 38
        d.ellipse(((x - 1.8) * S, (y - 1.8) * S, (x + 1.8) * S, (y + 1.8) * S), fill=(48, 36, 60, 255))
    d.ellipse(((cx - 31) * S, (cy - 31) * S, (cx + 31) * S, (cy + 31) * S), fill=(8, 6, 11, 255))
for x in (310, 476):
    d.line((x * S, 360 * S, x * S, 476 * S), fill=(48, 38, 60, 255), width=S)

# screws
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from knobgen import render_knob
screw = render_knob(int(14 * S), "alu", seed=9)
sd = ImageDraw.Draw(screw)
cs = screw.size[0] / 2
for a in (0.4, 0.4 + math.pi / 2):
    sd.line((cs - math.cos(a) * cs * 0.38, cs - math.sin(a) * cs * 0.38, cs + math.cos(a) * cs * 0.38, cs + math.sin(a) * cs * 0.38), fill=(40, 40, 44, 255), width=max(2, S))
for (x, y) in ((10, 10), (BW - 10, 10), (10, BH - 10), (BW - 10, BH - 10)):
    img.alpha_composite(screw, (int(x * S - cs), int(y * S - cs)))
d = ImageDraw.Draw(img)

out = os.path.join(ROOT, "Resources", "bg.png")
img.convert("RGB").save(out, optimize=True)
print("wrote", out, img.size)
